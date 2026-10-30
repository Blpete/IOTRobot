# IOTRobot

ESP8266 robot firmware, plus standalone sketches for the parts that are not integrated yet. Boards talk to an **MQTT broker that runs on a separate server on the LAN**. That broker is not in this repository. Its address is `MQTT_HOST` in the local config header.

License: [GPL-3.0-or-later](LICENSE). One file is an exception: `ssd1306_interface.ino/` is a modified Adafruit example and stays under the Adafruit BSD license in that folder's `license.txt`.

## Status

`iot_robot_main` is the robot. It brings up WiFi, MQTT, OTA, a 16×2 I2C LCD, a PCA9685 servo driver, and one L298N motor channel. Joystick axis Y and the Z button, published by the nunchuck example, drive that motor.

The other sketches are **part examples**. They compile and can be flashed on their own. They are not a second copy of the robot.

WiFi, MQTT, and OTA passwords are not in the source tree. Copy `include/config.example.h` to `include/config.h` (gitignored) and edit it before you flash a board onto a real network.

Passwords that used to be hardcoded are still in **git history**. Rotate the access-point passphrase and the MQTT password, and treat the old OTA-less firmware as untrusted. Do not reuse those secrets.

## Hardware

**TODO/assumption:** ESP8266 sketches target a NodeMCU 1.0 / v2 (ESP-12E). The editor files request `esp8266:esp8266:nodemcuv2`, 80 MHz, 4 MB flash with a 1 MB filesystem, upload 115200. Board revision is unconfirmed.

**TODO/assumption:** `commsController` targets a Heltec WiFi LoRa 32 class board (ESP32 + SX127x + SSD1306). V1, V2, and V3 are not confirmed. The pin numbers in that sketch match the V1 comments. V2 also needs Vext (GPIO21, active low). V3 uses different pins.

### Parts

| Part | Sketch | Notes |
|---|---|---|
| NodeMCU v2 (ESP8266) | Robot and the ESP8266 examples | **TODO/assumption** for every ESP8266 board. |
| L298N, one channel | `iot_robot_main`, `motorController` | Channel A only. Motor supply is separate from the NodeMCU 3.3 V pin. |
| PCA9685 at I2C `0x40` | `iot_robot_main` | Started at 60 Hz. No servo output is commanded yet. |
| 16×2 I2C LCD | Robot and joystick at `0x3F`; UI example at `0x27` | **TODO/assumption:** confirm which backpack is on which board. |
| Wii nunchuck | `joystickController` | I2C, `WiiChuck` library. **TODO/assumption:** power it at 3.3 V. |
| Heltec-style ESP32 + SX127x + SSD1306 `0x3C` | `commsController` | LoRa sender / MQTT forwarder. **TODO:** confirm the RF band before transmitting. The sketch uses 915 MHz. |
| SSD1306 128×32 | `ssd1306_interface.ino` | Counter demo. Not part of the robot. |

There is no RTC in the firmware. An old DS1307 include was a stub and has been removed.

## Wiring

ESP8266 `Wire.begin()` with no arguments uses **SDA = GPIO4 (NodeMCU D2)** and **SCL = GPIO5 (NodeMCU D1)**.

### L298N on the robot (and on the motor example)

| L298N | ESP8266 GPIO | NodeMCU | Notes |
|---|---|---|---|
| ENA | 14 | D5 | PWM speed. Remove the ENA jumper. |
| IN1 | 12 | D6 | Direction. |
| IN2 | 13 | D7 | Direction. |
| GND | GND | G | Common ground with the motor supply. |
| OUT1, OUT2 | motor leads | | Swap the leads if forward and back are reversed. |
| +12 V (motor supply) | external supply | | **TODO:** motor voltage. Do not feed the motor from the NodeMCU 3.3 V pin. |
| 5 V logic | module jumper or 5 V | | **TODO/assumption:** many L298N boards treat a 3.3 V ESP pin as a valid high. If the motor chatters, use a level shifter. |

IN3, IN4, and ENB are not connected. GPIO0, GPIO2, and GPIO4 are not motor pins: GPIO0 and GPIO2 are boot straps, and GPIO4 is I2C SDA.

Do not run `iot_robot_main` and `motorController` against the same L298N. Both use this pin map.

### I2C on the robot

| Device | Address |
|---|---|
| PCA9685 | `0x40` |
| 16×2 LCD | `0x3F` |

Pulse counts `150` and `600` (of 4096) are defined for later servo use and are not applied yet.

### Nunchuck example

SDA GPIO4, SCL GPIO5, LCD at `0x3F` if that example's display is fitted. The nunchuck address is the one the `WiiChuck` library uses (typically `0x52`).

### UI example

LCD at I2C `0x27`, 16×2. **TODO/assumption:** same default SDA/SCL.

### Comms example

| Function | GPIO |
|---|---|
| OLED SDA | 4 |
| OLED SCL | 15 |
| OLED RST | 16, held low 50 ms, then high |
| OLED address | `0x3C` |
| SX127x SCK | 5 |
| SX127x MISO | 19 |
| SX127x MOSI | 27 |
| SX127x CS | 18 |
| SX127x RST | 14 |
| SX127x DIO0 | 26 |
| Packet LED | 25 |

Radio settings in the sketch: carrier `915E6`, spreading factor 9, bandwidth `31.25E3`, coding rate 4/8, preamble 8. A 434 MHz define is commented out. **TODO:** confirm the module and the legal band. This bandwidth and spreading factor keep the transmitter on for a long time per short packet.

## Configuration

```bash
cp include/config.example.h include/config.h
```

Edit `include/config.h`. It is gitignored. The network library reads that file when the sketch is built from this repository. If it does not exist, the build uses the placeholders in `include/config.example.h`. A sketchbook copy of the library cannot see `include/config.h`; put the same contents in `libraries/IotRobotNet/src/iotrobot_config.h` next to the library sources.

| Macro | Meaning |
|---|---|
| `WIFI_SSID`, `WIFI_PASSWORD` | Access point. One SSID for every board. |
| `MQTT_HOST`, `MQTT_PORT` | Broker on the LAN server. Port `1883` is plaintext MQTT. There is no TLS in this firmware. |
| `MQTT_USER`, `MQTT_PASSWORD` | Broker account. |
| `OTA_PASSWORD` | ArduinoOTA password. Leave it non-empty. |

Placeholders are not usable secrets. Change them before a board joins a real network. Anyone who can publish to the topics below can move the motor, write the LCD, and write EEPROM. Anyone on the WiFi who knows the OTA password can replace the firmware.

Client ids are `<prefix>-<chipid>`, so two boards no longer share `ESP8266-1`. Prefixes: `robot`, `joy`, `ui`, `motor`, `lora`.

## Build and flash

Dependencies (Library Manager names, also installed by the CI script):

| Library | Sketches |
|---|---|
| PubSubClient | All networked sketches, via `IotRobotNet` |
| LiquidCrystal I2C (needs `init()`, `begin()`, `backlight()`) | Robot, joystick, UI |
| Adafruit PWM Servo Driver Library | Robot |
| WiiChuck | Joystick |
| LoRa by Sandeep Mistry | Comms |
| ESP8266 and ESP32 OLED driver for SSD1306 displays (`SSD1306.h`) | Comms |
| Adafruit GFX Library, Adafruit SSD1306 | OLED counter example |

Cores used by CI: ESP8266 3.1.2 and ESP32 2.0.17. **TODO/assumption:** the original editor paths named core 2.4.2. These sketches were built here against 3.1.2 and ESP32 2.0.17. On 3.1.2 the robot sketch uses about 95% of instruction RAM, so more code in that sketch can fail to link.

The LiquidCrystal I2C library CI installs (1.1.2, the YWROBOT port) is tagged as AVR in its metadata and prints a warning. It still provides `init()`, `begin()`, and `backlight()`, and the ESP8266 sketches build with it. ESP32 linking needs Python `pyserial` for that core's esptool; the build script installs it.

Compile everything:

```bash
bash ci/build-arduino/buildarduino.sh
```

One sketch:

```bash
arduino-cli compile --libraries "$PWD/libraries" \
  --fqbn esp8266:esp8266:nodemcuv2 \
  iot_robot_main
arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp8266:esp8266:nodemcuv2 \
  iot_robot_main
```

| Sketch | FQBN |
|---|---|
| `iot_robot_main`, `joystickController`, `uiController`, `motorController`, `ssd1306_interface.ino` | `esp8266:esp8266:nodemcuv2` |
| `commsController` | `esp32:esp32:esp32` |

**TODO/assumption:** a generic ESP32 FQBN is enough to compile the comms sketch. Pick the Heltec board entry that matches the silk screen before you flash it. The committed serial port `/dev/ttyS0` in `.vscode/` is a placeholder; use the real device (`/dev/ttyUSB0` or `/dev/ttyACM0`).

Serial monitor: **115200**, except the OLED counter example, which uses **9600**.

Arduino IDE does not see `libraries/IotRobotNet` unless that folder is on the library path. Either compile with the command above, or copy `libraries/IotRobotNet` into the sketchbook `libraries` folder and copy `include/config.h` to `libraries/IotRobotNet/src/iotrobot_config.h` inside that copy.

OTA runs only after WiFi connects, and only with `OTA_PASSWORD`. A failed access point no longer blocks `setup()` forever: WiFi is attempted for 15 seconds, MQTT for a few tries, then `loop()` keeps retrying every 10 seconds.

## Usage

Power the LAN broker, then the boards.

### Topics

| Topic | Who publishes | Who subscribes | Payload |
|---|---|---|---|
| `esp/input_x` | Joystick, when X changes | nobody yet | Decimal axis, about 0–255. |
| `esp/input_y` | Joystick, when Y changes | Robot | Decimal axis. Center is treated as 128. |
| `esp/button_z` | Joystick, on Z edges | Robot | `1` pressed, `0` released. Held Z forces the motor off. |
| `esp/msg` | anyone | Robot, joystick, UI, comms | Commands below. The UI prints the text. Comms forwards it over LoRa. |
| `esp/log` | Each networked sketch | nobody | Log lines. Joystick logs no longer go to `esp/msg`. |
| `esp/test` | Comms | nobody | `Sending packet` or `forwarded`. |
| `esp/motor-example` | anyone | Motor example only | `forward`, `back`, `stop`, `speed=<percent>`. |

**TODO/assumption:** nunchuck center is 128 and a larger Y means forward. Dead zone is ±24 counts. If the robot runs backward, swap the motor leads. Z held down is a cut, not a throttle.

### Commands on `esp/msg` (robot)

| Payload | Effect |
|---|---|
| `lcd-on` / `lcd-off` | Backlight. |
| `lcd-write0=<text>` / `lcd-write1=<text>` | Text at column 0 of that row. Rows are 0 and 1. |
| `lcd-clear` | Clear. |
| `pin1-on` / `pin1-off` | `LED_BUILTIN`, active low. This is not a motor pin. |
| `i2c-scan` | Scan I2C addresses 1–126. |
| `wifi-scan` | Print nearby SSIDs on serial. |
| `write-config=<text>` / `read-config` | EEPROM text, at most 253 bytes. |
| `motor-forward` / `motor-back` / `motor-stop` | L298N channel A. |
| `motor-speed=<percent>` | Percent from -100 to 100. |
| anything else | Printed on the LCD. |

`esp/input_x` is published for a later steering channel and is not used.

## What is integrated

- Robot: WiFi, MQTT, OTA, LCD at `0x3F`, PCA9685 `begin()`, L298N from joystick Y and from the motor commands.
- Joystick example publishes the nunchuck. The robot consumes Y and Z.
- UI example shows `esp/msg` on its own LCD.
- Motor example is a bench sketch on `esp/motor-example`.
- Comms example sends a LoRa counter every 2 seconds and forwards `esp/msg` out LoRa. It does not receive LoRa. If the radio fails to start, WiFi and MQTT keep running.
- OLED counter example scrolls a number and saves it to flash every 20 ticks.

## Project structure

```
.
├── LICENSE                         GPL-3.0
├── include/config.example.h        committed placeholders
├── include/config.h                gitignored, you create this
├── libraries/IotRobotNet/          WiFi, MQTT, OTA
├── iot_robot_main/                 robot sketch
├── joystickController/             nunchuck example
├── uiController/                   LCD example
├── motorController/                L298N bench example
├── commsController/                LoRa sender example
├── ssd1306_interface.ino/          Adafruit OLED example (BSD)
├── ci/build-arduino/buildarduino.sh
└── .github/workflows/build.yml
```

## License

Source files carry `SPDX-License-Identifier: GPL-3.0-or-later` and copyright Barry Peterson, except the OLED counter sketch.

`ssd1306_interface.ino/ssd1306_interface.ino.ino` is derived from Adafruit's SSD1306 128×32 I2C example, written by Limor Fried / Ladyada. That folder's `license.txt` is Adafruit's BSD license. Keep that notice with the file. The rest of this repository is GPL-3.0-or-later and is not an Adafruit product.
