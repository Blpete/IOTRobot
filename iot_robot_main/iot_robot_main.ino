// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

// Main robot sketch for a NodeMCU (ESP8266).
// WiFi, MQTT, and OTA live in IotRobotNet. This file is the LCD, the
// PCA9685 bring-up, and the L298N channel driven by the joystick.

#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_PWMServoDriver.h>
#include <IotRobotNet.h>

static const int EEPROM_SIZE = 256;

// L298N channel A. These are not boot-strap pins and not the I2C bus.
// NodeMCU labels: D5, D6, D7. Remove the ENA jumper so PWM reaches the driver.
static const int PIN_ENA = 14;
static const int PIN_IN1 = 12;
static const int PIN_IN2 = 13;

// Nunchuck axis is about 0..255 with the stick near the middle at rest.
// TODO/assumption: center is 128 and increasing Y is forward.
static const int JOY_CENTER = 128;
static const int JOY_DEAD = 24;

#define SERVO_PWM_MIN 150
#define SERVO_PWM_MAX 600

char DEVICE[12];
IotRobotNet net("robot");
LiquidCrystal_I2C lcd(0x3F, 16, 2);
Adafruit_PWMServoDriver servoDriver1(0x40);
bool lcdInitialized = false;
bool scanI2cRequested = false;
bool scanWifiRequested = false;
bool motorCut = false;
int lastAxis = JOY_CENTER;

void logMessage(const char* msg);
void onMqtt(char* topic, uint8_t* payload, unsigned int length);
void setupLcd();
void setupMotor();
void motorStop();
void applyAxis(int axis);
void driveFromAxis(int axis);
void executeCommand(const char* message);
void readConfig();
void writeConfig(const char* config);
void scanI2CBus();
void scanWifiNetworks();

void setup() {
  Serial.begin(115200);
  snprintf(DEVICE, sizeof(DEVICE), "%08lX", (unsigned long)ESP.getChipId());
  logMessage("Serial started");

  Wire.begin();
  setupLcd();
  servoDriver1.begin();
  servoDriver1.setPWMFreq(60);
  // SERVO_PWM_MIN / SERVO_PWM_MAX are the usual 12-bit pulse counts.
  // No servo channel is commanded yet.
  logMessage("Servo driver started");

  setupMotor();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  EEPROM.begin(EEPROM_SIZE);

  net.setHandler(onMqtt);
  net.begin();
  net.subscribe("esp/msg");
  net.subscribe("esp/input_y");
  net.subscribe("esp/button_z");
  logMessage("System initialized");
  logMessage(net.clientId());
}

void loop() {
  net.loop();
  if (scanI2cRequested) {
    scanI2cRequested = false;
    scanI2CBus();
  }
  if (scanWifiRequested) {
    scanWifiRequested = false;
    scanWifiNetworks();
  }
}

void onMqtt(char* topic, uint8_t* payload, unsigned int length) {
  char message[128];
  if (length >= sizeof(message)) {
    Serial.println("MQTT payload too long");
    return;
  }
  memcpy(message, payload, length);
  message[length] = '\0';

  if (strcmp(topic, "esp/input_y") == 0) {
    driveFromAxis(atoi(message));
    return;
  }
  if (strcmp(topic, "esp/button_z") == 0) {
    motorCut = (message[0] == '1');
    if (motorCut) {
      motorStop();
    } else {
      applyAxis(lastAxis);
    }
    return;
  }
  if (strcmp(topic, "esp/msg") == 0) {
    executeCommand(message);
  }
}

void setupLcd() {
  lcd.init();
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("I'm Alive");
  lcd.setCursor(0, 1);
  lcd.print(DEVICE);
  lcdInitialized = true;
}

void setupMotor() {
  digitalWrite(PIN_ENA, LOW);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  motorStop();
}

void motorStop() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_ENA, 0);
}

void applyAxis(int axis) {
  if (motorCut) {
    motorStop();
    return;
  }
  int delta = axis - JOY_CENTER;
  if (delta > -JOY_DEAD && delta < JOY_DEAD) {
    motorStop();
    return;
  }
  int span = 127 - JOY_DEAD;
  int mag = (abs(delta) - JOY_DEAD) * 1023 / span;
  if (mag < 0) {
    mag = 0;
  }
  if (mag > 1023) {
    mag = 1023;
  }
  if (delta > 0) {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
  } else {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
  }
  analogWrite(PIN_ENA, mag);
}

void driveFromAxis(int axis) {
  lastAxis = axis;
  applyAxis(axis);
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
  if (strcmp(message, "lcd-off") == 0) {
    if (lcdInitialized) {
      lcd.noBacklight();
    }
  } else if (strcmp(message, "lcd-on") == 0) {
    if (lcdInitialized) {
      lcd.backlight();
    }
  } else if (strncmp(message, "lcd-write0", 10) == 0) {
    if (lcdInitialized) {
      lcd.setCursor(0, 0);
      lcd.print(param);
    }
  } else if (strncmp(message, "lcd-write1", 10) == 0) {
    if (lcdInitialized) {
      lcd.setCursor(0, 1);
      lcd.print(param);
    }
  } else if (strncmp(message, "lcd-clear", 9) == 0) {
    if (lcdInitialized) {
      lcd.clear();
    }
  } else if (strcmp(message, "pin1-on") == 0) {
    digitalWrite(LED_BUILTIN, LOW);
  } else if (strcmp(message, "pin1-off") == 0) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (strncmp(message, "i2c-scan", 8) == 0) {
    scanI2cRequested = true;
  } else if (strcmp(message, "wifi-scan") == 0) {
    scanWifiRequested = true;
  } else if (strncmp(message, "write-config", 12) == 0) {
    writeConfig(param);
  } else if (strncmp(message, "read-config", 11) == 0) {
    readConfig();
  } else if (strcmp(message, "motor-stop") == 0) {
    motorCut = true;
    motorStop();
  } else if (strcmp(message, "motor-forward") == 0) {
    motorCut = false;
    driveFromAxis(JOY_CENTER + 80);
  } else if (strcmp(message, "motor-back") == 0) {
    motorCut = false;
    driveFromAxis(JOY_CENTER - 80);
  } else if (strncmp(message, "motor-speed=", 12) == 0) {
    int percent = atoi(param);
    if (percent > 100) {
      percent = 100;
    }
    if (percent < -100) {
      percent = -100;
    }
    motorCut = false;
    if (percent == 0) {
      driveFromAxis(JOY_CENTER);
    } else if (percent > 0) {
      driveFromAxis(JOY_CENTER + JOY_DEAD + (percent * (127 - JOY_DEAD)) / 100);
    } else {
      driveFromAxis(JOY_CENTER - JOY_DEAD + (percent * (127 - JOY_DEAD)) / 100);
    }
  } else if (lcdInitialized) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
  }
}

void readConfig() {
  int size = (EEPROM.read(0) << 8) | EEPROM.read(1);
  if (size < 0 || size > EEPROM_SIZE - 3) {
    logMessage("config length rejected");
    return;
  }
  if (size == 0) {
    logMessage("config empty");
    return;
  }
  char temp[EEPROM_SIZE];
  for (int i = 0; i < size; i++) {
    temp[i] = EEPROM.read(i + 2);
  }
  temp[size] = '\0';
  logMessage("config:");
  logMessage(temp);
}

void writeConfig(const char* config) {
  if (config == nullptr) {
    return;
  }
  int len = strlen(config);
  if (len > EEPROM_SIZE - 3) {
    logMessage("config too long");
    return;
  }
  EEPROM.write(0, (len >> 8) & 0xFF);
  EEPROM.write(1, len & 0xFF);
  for (int i = 0; i < len; i++) {
    EEPROM.write(i + 2, config[i]);
  }
  if (!EEPROM.commit()) {
    logMessage("config commit failed");
    return;
  }
  char note[32];
  snprintf(note, sizeof(note), "config wrote %d", len);
  logMessage(note);
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

void scanWifiNetworks() {
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n <= 0) {
    Serial.println("no networks found");
    return;
  }
  Serial.print(n);
  Serial.println(" networks found");
  for (int i = 0; i < n; i++) {
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.print(")");
    Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
    delay(10);
  }
}
