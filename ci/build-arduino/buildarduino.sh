#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Compile each IOTRobot sketch for the board it targets.
# Placeholder WiFi/MQTT settings in include/config.example.h are enough
# to compile. A gitignored include/config.h is picked up when present.

set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

CLI_VERSION="1.2.2"
ESP8266_CORE="esp8266:esp8266@3.1.2"
ESP32_CORE="esp32:esp32@2.0.17"
FQBN_8266="esp8266:esp8266:nodemcuv2"
FQBN_32="esp32:esp32:esp32"

install_cli() {
  if command -v arduino-cli >/dev/null 2>&1; then
    return
  fi
  local tmp dest
  tmp="$(mktemp -d)"
  curl -fsSL -o "$tmp/arduino-cli.tar.gz" \
    "https://github.com/arduino/arduino-cli/releases/download/v${CLI_VERSION}/arduino-cli_${CLI_VERSION}_Linux_64bit.tar.gz"
  tar -xzf "$tmp/arduino-cli.tar.gz" -C "$tmp"
  if [ -w /usr/local/bin ]; then
    dest=/usr/local/bin/arduino-cli
  else
    mkdir -p "$HOME/.local/bin"
    dest="$HOME/.local/bin/arduino-cli"
    export PATH="$HOME/.local/bin:$PATH"
  fi
  install -m 0755 "$tmp/arduino-cli" "$dest"
  rm -rf "$tmp"
}

ensure_config() {
  if ! arduino-cli config dump >/dev/null 2>&1; then
    arduino-cli config init
  fi
  arduino-cli config add board_manager.additional_urls \
    https://arduino.esp8266.com/stable/package_esp8266com_index.json || true
  arduino-cli config add board_manager.additional_urls \
    https://espressif.github.io/arduino-esp32/package_esp32_index.json || true
}

install_cores() {
  arduino-cli core update-index
  arduino-cli core install "$ESP8266_CORE"
  arduino-cli core install "$ESP32_CORE"
}

install_python_deps() {
  # ESP32 core 2.0.17's esptool imports pyserial at the end of the link.
  python3 -m pip install --user pyserial \
    || python3 -m pip install --break-system-packages pyserial
}

install_libs() {
  arduino-cli lib update-index
  arduino-cli lib install \
    "PubSubClient" \
    "LiquidCrystal I2C" \
    "Adafruit PWM Servo Driver Library" \
    "Adafruit GFX Library" \
    "Adafruit SSD1306" \
    "LoRa" \
    "WiiChuck" \
    "ESP8266 and ESP32 OLED driver for SSD1306 displays"
}

compile_one() {
  local fqbn="$1"
  local sketch="$2"
  echo "=== compile $sketch ($fqbn) ==="
  arduino-cli compile --warnings all \
    --libraries "$ROOT/libraries" \
    --fqbn "$fqbn" \
    "$ROOT/$sketch"
}

install_cli
ensure_config
install_cores
install_libs
install_python_deps

fail=0
for sketch in iot_robot_main joystickController uiController motorController ssd1306_interface.ino; do
  if ! compile_one "$FQBN_8266" "$sketch"; then
    fail=1
  fi
done
if ! compile_one "$FQBN_32" commsController; then
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  echo "One or more sketches failed to compile"
  exit 1
fi
echo "All sketches compiled"
