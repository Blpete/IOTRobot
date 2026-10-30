// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

// Part example: Wii nunchuck publisher. Not integrated beyond MQTT.
// Publishes esp/input_x, esp/input_y, and esp/button_z. The main robot
// sketch subscribes to Y and Z.

#include <Wire.h>
#include <WiiChuck.h>
#include <LiquidCrystal_I2C.h>
#include <IotRobotNet.h>

char DEVICE[12];
IotRobotNet net("joy");
LiquidCrystal_I2C lcd(0x3F, 16, 2);
Accessory nunchuck1;
bool lcdInitialized = false;
bool haveSample = false;
bool buttonZ = false;
int lastX = 0;
int lastY = 0;
unsigned long lastNunchuckMs = 0;

void logMessage(const char* msg);
void onMqtt(char* topic, uint8_t* payload, unsigned int length);
void setupLcd();
void sendNunchuck();
void executeCommand(const char* message);
void scanI2CBus();

void setup() {
  Serial.begin(115200);
  snprintf(DEVICE, sizeof(DEVICE), "%08lX", (unsigned long)ESP.getChipId());
  logMessage("Serial started");

  Wire.begin();
  setupLcd();
  scanI2CBus();

  nunchuck1.begin();
  if (nunchuck1.type == UnknownChuck) {
    nunchuck1.type = NUNCHUCK;
  }
  logMessage("Nunchuck init");

  net.setHandler(onMqtt);
  net.begin();
  net.subscribe("esp/msg");
  logMessage(net.clientId());
}

void loop() {
  net.loop();
  if (millis() - lastNunchuckMs >= 50) {
    lastNunchuckMs = millis();
    sendNunchuck();
  }
}

void onMqtt(char* topic, uint8_t* payload, unsigned int length) {
  char message[128];
  if (strcmp(topic, "esp/msg") != 0) {
    return;
  }
  if (length >= sizeof(message)) {
    Serial.println("MQTT payload too long");
    return;
  }
  memcpy(message, payload, length);
  message[length] = '\0';
  executeCommand(message);
}

void sendNunchuck() {
  nunchuck1.readData();
  if (!net.mqttConnected()) {
    return;
  }
  int x = nunchuck1.getJoyX();
  int y = nunchuck1.getJoyY();
  bool z = nunchuck1.getButtonZ();
  char buf[8];

  if (!haveSample || abs(x - lastX) >= 2) {
    snprintf(buf, sizeof(buf), "%d", x);
    net.publish("esp/input_x", buf);
    lastX = x;
  }
  if (!haveSample || abs(y - lastY) >= 2) {
    snprintf(buf, sizeof(buf), "%d", y);
    net.publish("esp/input_y", buf);
    lastY = y;
  }
  if (!haveSample || z != buttonZ) {
    net.publish("esp/button_z", z ? "1" : "0");
    logMessage(z ? "Z pressed" : "Z released");
    buttonZ = z;
  }
  haveSample = true;
}

void setupLcd() {
  lcd.init();
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Joy");
  lcd.setCursor(0, 1);
  lcd.print(DEVICE);
  lcdInitialized = true;
}

void logMessage(const char* msg) {
  Serial.println(msg);
  if (!net.inCallback()) {
    net.publish("esp/log", msg);
  }
  if (lcdInitialized) {
    lcd.setCursor(0, 1);
    lcd.print(msg);
  }
}

const char* argAfter(const char* message) {
  const char* p = strchr(message, '=');
  if (p == nullptr || p == message) {
    return "";
  }
  return p + 1;
}

void executeCommand(const char* message) {
  const char* param = argAfter(message);
  if (!lcdInitialized) {
    return;
  }
  if (strcmp(message, "lcd-off") == 0) {
    lcd.noBacklight();
  } else if (strcmp(message, "lcd-on") == 0) {
    lcd.backlight();
  } else if (strncmp(message, "lcd-write0", 10) == 0) {
    lcd.setCursor(0, 0);
    lcd.print(param);
  } else if (strncmp(message, "lcd-write1", 10) == 0) {
    lcd.setCursor(0, 1);
    lcd.print(param);
  } else if (strncmp(message, "lcd-clear", 9) == 0) {
    lcd.clear();
  } else if (strcmp(message, "pin1-on") == 0) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  } else if (strcmp(message, "pin1-off") == 0) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (strncmp(message, "i2c-scan", 8) == 0) {
    scanI2CBus();
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
