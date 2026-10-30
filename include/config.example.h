#pragma once

// Included by libraries/IotRobotNet. Do not remove this macro; it keeps
// the placeholder header from being compiled in as well.
#define IOTROBOT_CONFIG_LOADED 1

// Copy this file to include/config.h and edit the placeholders.
// include/config.h is gitignored. Do not commit real passwords.
//
// The MQTT broker is a separate server on the LAN. It is not part of
// this repository. MQTT_HOST is that server's address.
//
// These placeholders let the firmware compile. Change them before
// flashing a board that joins a real network. An OTA password is
// required; leave it non-empty.

static const char WIFI_SSID[] = "your-ssid";
static const char WIFI_PASSWORD[] = "your-wifi-passphrase";
static const char MQTT_HOST[] = "192.168.1.10";
static const uint16_t MQTT_PORT = 1883;
static const char MQTT_USER[] = "mqtt-user";
static const char MQTT_PASSWORD[] = "mqtt-password";
static const char OTA_PASSWORD[] = "change-this-ota-password";
