// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#include "IotRobotNet.h"

#include <ArduinoOTA.h>
#include <string.h>

// Gitignored override when this library is built from the repo tree.
// A sketchbook copy can instead provide iotrobot_config.h beside this file.
// The fallback is config.example.h (placeholders, safe to compile).
#if __has_include("../../../include/config.h")
#include "../../../include/config.h"
#endif
#ifndef IOTROBOT_CONFIG_LOADED
#if __has_include("iotrobot_config.h")
#include "iotrobot_config.h"
#endif
#endif
#ifndef IOTROBOT_CONFIG_LOADED
#include "config.example.h"
#endif

namespace {

const unsigned long kWifiGiveUpMs = 15000;
const unsigned long kRetryMs = 10000;

}  // namespace

IotRobotNet* IotRobotNet::active_ = nullptr;

IotRobotNet::IotRobotNet(const char* namePrefix)
    : prefix_(namePrefix),
      wifi_(),
      mqtt_(wifi_),
      subCount_(0),
      handler_(nullptr),
      inCallback_(false),
      otaStarted_(false),
      lastWifiAttempt_(0),
      lastMqttAttempt_(0) {
  clientId_[0] = '\0';
  for (int i = 0; i < kMaxSubscriptions; i++) {
    subs_[i] = nullptr;
  }
  active_ = this;
}

void IotRobotNet::buildClientId() {
  uint32_t id = 0;
#if defined(ESP8266)
  id = ESP.getChipId();
#elif defined(ESP32)
  id = (uint32_t)ESP.getEfuseMac();
#endif
  snprintf(clientId_, sizeof(clientId_), "%s-%08lX", prefix_, (unsigned long)id);
}

void IotRobotNet::begin() {
  buildClientId();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  Serial.print("WiFi SSID ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiGiveUpMs) {
    delay(200);
  }
  lastWifiAttempt_ = millis();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi IP ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected; continuing");
  }

  mqtt_.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_.setCallback(onMqtt);
  ensureOta();
  if (WiFi.status() == WL_CONNECTED) {
    for (int attempt = 0; attempt < 3 && !mqtt_.connected(); attempt++) {
      if (!connectMqtt()) {
        delay(500);
      }
    }
  }
  lastMqttAttempt_ = millis();
}

void IotRobotNet::ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (millis() - lastWifiAttempt_ < kRetryMs) {
    return;
  }
  lastWifiAttempt_ = millis();
  Serial.println("WiFi reconnect");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void IotRobotNet::ensureOta() {
  if (otaStarted_ || WiFi.status() != WL_CONNECTED) {
    return;
  }
  ArduinoOTA.setHostname(clientId_);
  if (OTA_PASSWORD[0] != '\0') {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() {
    Serial.println(ArduinoOTA.getCommand() == U_FLASH ? "OTA sketch" : "OTA filesystem");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA end"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int pct = total ? (progress * 100UL) / total : 0;
    Serial.printf("Progress: %u%%\r", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]\n", error);
  });
  ArduinoOTA.begin();
  otaStarted_ = true;
  Serial.print("OTA hostname ");
  Serial.println(clientId_);
}

bool IotRobotNet::connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (mqtt_.connected()) {
    return true;
  }
  Serial.print("MQTT connect as ");
  Serial.println(clientId_);
  if (!mqtt_.connect(clientId_, MQTT_USER, MQTT_PASSWORD)) {
    Serial.print("MQTT failed, state ");
    Serial.println(mqtt_.state());
    return false;
  }
  for (int i = 0; i < subCount_; i++) {
    mqtt_.subscribe(subs_[i]);
  }
  Serial.print("MQTT broker ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  return true;
}

void IotRobotNet::loop() {
  ensureWifi();
  ensureOta();
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (!mqtt_.connected()) {
    if (millis() - lastMqttAttempt_ >= kRetryMs) {
      lastMqttAttempt_ = millis();
      connectMqtt();
    }
    return;
  }
  mqtt_.loop();
}

bool IotRobotNet::wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool IotRobotNet::mqttConnected() {
  return mqtt_.connected();
}

bool IotRobotNet::inCallback() const {
  return inCallback_;
}

const char* IotRobotNet::clientId() const {
  return clientId_;
}

bool IotRobotNet::publish(const char* topic, const char* payload) {
  if (inCallback_ || !mqtt_.connected() || topic == nullptr || payload == nullptr) {
    return false;
  }
  return mqtt_.publish(topic, payload);
}

void IotRobotNet::subscribe(const char* topic) {
  if (topic == nullptr) {
    return;
  }
  for (int i = 0; i < subCount_; i++) {
    if (strcmp(subs_[i], topic) == 0) {
      if (mqtt_.connected()) {
        mqtt_.subscribe(topic);
      }
      return;
    }
  }
  if (subCount_ >= kMaxSubscriptions) {
    Serial.println("MQTT subscription list full");
    return;
  }
  subs_[subCount_++] = topic;
  if (mqtt_.connected()) {
    mqtt_.subscribe(topic);
  }
}

void IotRobotNet::setHandler(Handler handler) {
  handler_ = handler;
}

void IotRobotNet::onMqtt(char* topic, uint8_t* payload, unsigned int length) {
  if (active_ == nullptr || active_->handler_ == nullptr) {
    return;
  }
  active_->inCallback_ = true;
  active_->handler_(topic, payload, length);
  active_->inCallback_ = false;
}
