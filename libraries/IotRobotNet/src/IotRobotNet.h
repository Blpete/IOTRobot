// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#error "IotRobotNet supports ESP8266 and ESP32 only"
#endif

// One IotRobotNet object per board. Topics passed to subscribe() must
// remain valid for the life of the object (string literals are fine).
class IotRobotNet {
 public:
  static const int kMaxSubscriptions = 6;
  typedef void (*Handler)(char* topic, uint8_t* payload, unsigned int length);

  explicit IotRobotNet(const char* namePrefix);

  // Join WiFi and try MQTT, each with a timeout. Returns to the caller
  // even if either one is down so setup() can reach loop().
  void begin();
  void loop();

  bool wifiConnected();
  bool mqttConnected();
  bool inCallback() const;
  const char* clientId() const;

  bool publish(const char* topic, const char* payload);
  void subscribe(const char* topic);
  void setHandler(Handler handler);

 private:
  static IotRobotNet* active_;
  static void onMqtt(char* topic, uint8_t* payload, unsigned int length);

  void buildClientId();
  void ensureWifi();
  void ensureOta();
  bool connectMqtt();

  const char* prefix_;
  WiFiClient wifi_;
  PubSubClient mqtt_;
  char clientId_[20];
  const char* subs_[kMaxSubscriptions];
  int subCount_;
  Handler handler_;
  bool inCallback_;
  bool otaStarted_;
  unsigned long lastWifiAttempt_;
  unsigned long lastMqttAttempt_;
};
