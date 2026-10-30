// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

// Part example: one L298N channel on a spare NodeMCU.
// This is not the robot. The robot drives the same pin map from
// iot_robot_main. Do not run both against one driver.
//
// Commands on esp/motor-example:
//   forward
//   back
//   stop
//   speed=<percent>   percent is -100..100

#include <IotRobotNet.h>

// NodeMCU D5, D6, D7. Not GPIO0, GPIO2, or GPIO4 (boot straps and I2C).
static const int PIN_ENA = 14;
static const int PIN_IN1 = 12;
static const int PIN_IN2 = 13;

IotRobotNet net("motor");

void logMessage(const char* msg);
void onMqtt(char* topic, uint8_t* payload, unsigned int length);
void setupMotor();
void motorStop();
void motorDrive(int speedSigned);
void handleCommand(const char* message);

void setup() {
  Serial.begin(115200);
  setupMotor();
  net.setHandler(onMqtt);
  net.begin();
  net.subscribe("esp/motor-example");
  logMessage("L298N example");
  logMessage(net.clientId());
}

void loop() {
  net.loop();
}

void onMqtt(char* topic, uint8_t* payload, unsigned int length) {
  char message[64];
  if (strcmp(topic, "esp/motor-example") != 0) {
    return;
  }
  if (length >= sizeof(message)) {
    Serial.println("MQTT payload too long");
    return;
  }
  memcpy(message, payload, length);
  message[length] = '\0';
  handleCommand(message);
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

void motorDrive(int speedSigned) {
  if (speedSigned > 1023) {
    speedSigned = 1023;
  }
  if (speedSigned < -1023) {
    speedSigned = -1023;
  }
  if (speedSigned == 0) {
    motorStop();
    return;
  }
  if (speedSigned > 0) {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
    analogWrite(PIN_ENA, speedSigned);
  } else {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
    analogWrite(PIN_ENA, -speedSigned);
  }
}

void handleCommand(const char* message) {
  if (strcmp(message, "stop") == 0) {
    motorStop();
    logMessage("stop");
  } else if (strcmp(message, "forward") == 0) {
    motorDrive(700);
    logMessage("forward");
  } else if (strcmp(message, "back") == 0) {
    motorDrive(-700);
    logMessage("back");
  } else if (strncmp(message, "speed=", 6) == 0) {
    int percent = atoi(message + 6);
    if (percent > 100) {
      percent = 100;
    }
    if (percent < -100) {
      percent = -100;
    }
    motorDrive(percent * 1023 / 100);
    logMessage("speed");
  } else {
    logMessage("ignored");
  }
}

void logMessage(const char* msg) {
  Serial.println(msg);
  if (!net.inCallback()) {
    net.publish("esp/log", msg);
  }
}
