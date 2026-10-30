// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

// Part example: 16x2 I2C LCD that prints esp/msg. Not the robot controller.
// Backpack address 0x27 is what this sketch was written against.
// TODO/assumption: confirm the backpack on the bench. The main sketch uses 0x3F.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IotRobotNet.h>

IotRobotNet net("ui");
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdReady = false;
bool showedIp = false;

void logMessage(const char* msg);
void onMqtt(char* topic, uint8_t* payload, unsigned int length);
void scanI2CBus();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  lcdReady = true;

  scanI2CBus();

  net.setHandler(onMqtt);
  net.begin();
  net.subscribe("esp/msg");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("waiting...");
  logMessage(net.clientId());
}

void loop() {
  net.loop();
  if (!showedIp && net.wifiConnected()) {
    showedIp = true;
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  }
}

void onMqtt(char* topic, uint8_t* payload, unsigned int length) {
  char message[128];
  if (strcmp(topic, "esp/msg") != 0 || !lcdReady) {
    return;
  }
  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.println(message);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}

void logMessage(const char* msg) {
  Serial.println(msg);
  if (!net.inCallback()) {
    net.publish("esp/log", msg);
  }
}

void scanI2CBus() {
  int nDevices = 0;
  logMessage("Scanning I2C...");
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      char line[40];
      snprintf(line, sizeof(line), "I2C found at 0x%02X", address);
      logMessage(line);
      nDevices++;
    } else if (error == 4) {
      char line[40];
      snprintf(line, sizeof(line), "I2C error at 0x%02X", address);
      logMessage(line);
    }
  }
  if (nDevices == 0) {
    logMessage("No I2C devices found");
  } else {
    logMessage("I2C scan done");
  }
}
