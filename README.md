# IOTRobot

ESP32 robot firmware: Wi-Fi, MQTT, a Wii nunchuck on I2C, and one L298N motor channel. This is a **native ESP-IDF** application. It does not use the Arduino core or Arduino libraries.

The MQTT broker is a **separate server on the LAN**. It is not in this repository. Point `CONFIG_IOTROBOT_MQTT_URI` at that machine.

License: [GPL-3.0-or-later](LICENSE).

Passwords that were hardcoded in older revisions are still in **git history**. Rotate the access-point passphrase and the MQTT password. Do not reuse them.

## ESP8266 is not supported

ESP-IDF does not build for the ESP8266. Every sketch in the previous tree except `commsController` was an ESP8266 (NodeMCU) sketch. Those boards cannot run this firmware.

Use a classic **ESP32** module (ESP32-WROOM-32 class), not an ESP8266, ESP32-S2, S3, or C3. The build target is `esp32`. Flash size is assumed to be **4 MB**.

`commsController` was already an ESP32 sketch (Heltec WiFi LoRa 32). Its SSD1306 OLED and SX127x LoRa radio are **not ported**. Board revision (V1 / V2 / V3) was never confirmed. Do not flash this image expecting the old LoRa sender.

## What this firmware does

`app_main` starts FreeRTOS tasks and returns. Wi-Fi and MQTT run on the ESP-IDF event loop. They do not block the motor task.

- Station Wi-Fi joins the configured SSID. If the association drops, a timer calls `esp_wifi_connect()` again after 5 seconds.
- `esp-mqtt` connects to the LAN broker and reconnects on its own. The client id is `robot-` plus the Wi-Fi MAC (`robot-AABBCCDDEEFF`).
- The nunchuck is read on I2C (address `0x52`) about every 50 ms. Stick Y drives the motor. Z held down forces the motor off.
- If the nunchuck is missing, MQTT commands on `esp/msg` drive the motor instead.
- While the stick is inside the dead zone, an MQTT speed command is used. Moving the stick out of the dead zone takes over.

| Topic | Direction | Payload |
|---|---|---|
| `esp/input_x` | publish, when X changes | Decimal 0–255. Not used for steering. |
| `esp/input_y` | publish, when Y changes | Decimal 0–255. |
| `esp/button_z` | publish, on edges | `1` pressed, `0` released. |
| `esp/log` | publish | `online robot-...`, `Z pressed`, `Z released`. |
| `esp/msg` | subscribe | `motor-stop`, `motor-forward`, `motor-back`, `motor-speed=<percent>` (`-100`..`100`). Other text is logged and ignored. |

**TODO:** nunchuck center is 128 and a larger Y means forward. Swap the motor leads if the direction is wrong. Dead zone default is ±24 counts.

## Wiring

Common ground between the ESP32, the L298N logic, and the motor supply. Do not power the motor from the ESP32 3.3 V pin. Motor voltage is **TODO**.

### L298N channel A

Defaults match the pin numbers from the ESP8266 firmware. Change them with `idf.py menuconfig` under **IOTRobot**.

| L298N | Default GPIO | Notes |
|---|---|---|
| ENA | 14 | 10-bit PWM, default 1 kHz. Remove the ENA jumper. |
| IN1 | 12 | Direction. |
| IN2 | 13 | Direction. |
| GND | GND | Common ground. |
| OUT1, OUT2 | motor | Swap the leads to reverse forward. |
| Motor supply | external | Not the ESP32 3.3 V pin. |
| Logic 5 V | module jumper | **TODO:** many boards accept 3.3 V as a high. If the motor chatters, use a level shifter. |

**GPIO12 is an ESP32 strapping pin.** If it is high at reset, the flash is powered at 1.8 V and the chip will not boot. The firmware drives IN1 low after boot, but the L298N must not hold GPIO12 high while the ESP32 is resetting. If the board never boots, set IN1 to GPIO27 (or another non-strapping pin) in menuconfig. GPIO0, GPIO2, and GPIO15 are also poor choices.

IN3, IN4, and ENB are not used.

### Nunchuck

| Signal | Default GPIO |
|---|---|
| SDA | 21 |
| SCL | 22 |
| VCC | 3.3 V |
| GND | GND |

The old ESP8266 sketches used GPIO4 and GPIO5. Those are not the defaults here. Address `0x52`, 100 kHz. Internal pull-ups are enabled; add external 4.7 kΩ pull-ups if the cable is long. The init sequence (`0xF0=0x55`, `0xFB=0x00`) turns encryption off.

## What was not ported

The old Arduino sketches are removed. They were separate part experiments, not a second robot.

| Old sketch | Board | Fate |
|---|---|---|
| `iot_robot_main` | ESP8266 | Replaced by this ESP-IDF app. LCD, PCA9685 servo driver, OTA, and EEPROM config were not ported. |
| `joystickController` | ESP8266 | The nunchuck is on this ESP32's I2C bus. There is no second board publishing joystick topics. |
| `motorController` | ESP8266 | The L298N driver is in this app. |
| `uiController` | ESP8266 | Dropped. No LCD. |
| `commsController` | ESP32 Heltec | Dropped. No SSD1306 and no LoRa. |
| `ssd1306_interface.ino` | ESP8266 | Removed. It was a modified Adafruit demo. No Adafruit-derived source remains in this tree. |

## Configure secrets

Copy the example and edit the placeholders. Do not commit the copy.

```bash
cp sdkconfig.defaults.private.example sdkconfig.defaults.private
```

On Windows the same file name is used. `build.sh` / `build.ps1` pass it to ESP-IDF as `SDKCONFIG_DEFAULTS`. If that file is newer than `sdkconfig`, the scripts delete `sdkconfig` so the new values are applied.

You can also change the same settings with menuconfig (the **IOTRobot** menu):

```bash
./build.sh idf.py menuconfig
```

```powershell
.\build.ps1 idf.py menuconfig
```

`sdkconfig` is gitignored. A build with no private file and an empty SSID still compiles. The chip then runs the motor task and skips Wi-Fi.

## Build on Windows (Podman Desktop)

ESP-IDF is pinned in `idf-version.txt` (currently v5.3.2). The image is `docker.io/espressif/idf:<that version>`.

1. Install [Podman Desktop](https://podman-desktop.io/) and start the machine. Docker works too; the script uses `podman` if it is on `PATH`, otherwise `docker`.
2. Open PowerShell in this folder.
3. Copy and edit `sdkconfig.defaults.private` (above).
4. Build:

```powershell
.\build.ps1
```

That runs:

```text
podman run --rm -v <repo>:/project -w /project docker.io/espressif/idf:v5.3.2 idf.py set-target esp32 build
```

The first run downloads the image (several GB). Override the image with the `IDF_IMAGE` environment variable if you need to.

A debug build with ESP-IDF v5.3.2 (`idf.py size`) is:

| Region | Used | Of |
|---|---:|---:|
| App image | 525,516 bytes | 1,048,576-byte factory app partition (about 50% free; the flashed `.bin` is 525,632 bytes) |
| IRAM | 58,519 bytes (45%) | 131,072 |
| DRAM | 15,548 bytes (8.6%) | 180,736 (`.data` 9,588, `.bss` 5,960) |

Linux and macOS:

```bash
./build.sh
```

## Flash and monitor from Windows

The container build does not see your serial port. Flash from the host. Install esptool once:

```powershell
py -m pip install esptool
```

`build/flash_args` paths are relative to `build/`. From that directory, replace `COM5` with the port shown in Device Manager:

```powershell
cd build
py -m esptool --chip esp32 --port COM5 --baud 460800 write_flash "@flash_args"
```

Serial log is 115200. esptool installs pyserial, so:

```powershell
py -m serial.tools.miniterm COM5 115200
```

Leave miniterm with `Ctrl+]`.

Nothing in this port has been flashed to a board. Wi-Fi, MQTT, the nunchuck, and the L298N are untested on hardware.

## Project layout

```
CMakeLists.txt
idf-version.txt
sdkconfig.defaults
sdkconfig.defaults.private.example
main/                 application (Wi-Fi, MQTT, nunchuck, L298N)
build.sh
build.ps1
.github/workflows/build.yml
```

Wi-Fi, MQTT, I2C, GPIO, and LEDC come from ESP-IDF. There is no `components/` directory and no Arduino component.

## License

Source files are `SPDX-License-Identifier: GPL-3.0-or-later`, copyright Barry Peterson.

The previous `ssd1306_interface.ino` tree was a modified Adafruit SSD1306 example under Adafruit's BSD license. That example was removed rather than rewritten. No Adafruit-derived code is in this tree.
