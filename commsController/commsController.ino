// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

// Part example: ESP32 LoRa sender and MQTT-to-LoRa forwarder.
// TODO/assumption: pin numbers match a Heltec WiFi LoRa 32 V1.
// V2 needs the Vext rail (GPIO21, active low) enabled. V3 uses other pins.
// Board revision is unconfirmed. Carrier is 915 MHz; confirm the module
// before transmitting. A 434 MHz option is recorded below and not used.

#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include "SSD1306.h"
#include <IotRobotNet.h>

// OLED: SDA GPIO4, SCL GPIO15, RST GPIO16, address 0x3C.
SSD1306 display(0x3c, 4, 15);

// SX127x: SCK 5, MISO 19, MOSI 27, CS 18, RST 14, DIO0 26.
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_BAND 915E6
// #define LORA_BAND 434500000
#define LORA_SPREADING_FACTOR 9
#define LORA_BANDWIDTH 31.25E3
#define LORA_PREAMBLE 8
#define LORA_CODING_RATE 8
#define PIN_LED 25

IotRobotNet net("lora");

bool loraReady = false;
int counter = 0;
unsigned long lastPacketMs = 0;
unsigned long ledOffMs = 0;
bool loraQueued = false;
char loraMessage[48];

void onMqtt(char* topic, uint8_t* payload, unsigned int length);
void showLine(int y, const char* text);
void sendCounter();
void sendQueued();

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif

  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  delay(50);
  digitalWrite(16, HIGH);

  Serial.begin(115200);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, "LoRa sender");
  display.display();

  net.setHandler(onMqtt);
  net.begin();
  net.subscribe("esp/msg");
  showLine(12, net.clientId());

  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("Starting LoRa failed");
    showLine(24, "LoRa failed");
    loraReady = false;
    return;
  }
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setPreambleLength(LORA_PREAMBLE);
  loraReady = true;
  Serial.println("LoRa ready");
  showLine(24, "LoRa ready");
}

void loop() {
  net.loop();
  if (ledOffMs != 0 && millis() >= ledOffMs) {
    digitalWrite(PIN_LED, LOW);
    ledOffMs = 0;
  }
  if (!loraReady) {
    return;
  }
  sendQueued();
  if (millis() - lastPacketMs >= 2000) {
    lastPacketMs = millis();
    sendCounter();
  }
}

void onMqtt(char* topic, uint8_t* payload, unsigned int length) {
  if (strcmp(topic, "esp/msg") != 0) {
    return;
  }
  unsigned int n = length;
  if (n >= sizeof(loraMessage)) {
    n = sizeof(loraMessage) - 1;
  }
  if (n > 0) {
    memcpy(loraMessage, payload, n);
  }
  loraMessage[n] = '\0';
  loraQueued = true;
#ifdef LED_BUILTIN
  if (n > 0 && loraMessage[0] == '1') {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
#endif
}

void showLine(int y, const char* text) {
  display.drawString(0, y, text);
  display.display();
}

void pulseLed() {
  digitalWrite(PIN_LED, HIGH);
  ledOffMs = millis() + 200;
}

void sendCounter() {
  Serial.print("Sending packet: ");
  Serial.println(counter);
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Sending");
  char countBuf[12];
  snprintf(countBuf, sizeof(countBuf), "%d", counter);
  display.drawString(0, 24, countBuf);
  display.display();

  LoRa.beginPacket();
  LoRa.print("Hello..");
  LoRa.print(counter);
  LoRa.endPacket();
  counter++;
  pulseLed();
  net.publish("esp/test", "Sending packet");
}

void sendQueued() {
  if (!loraQueued) {
    return;
  }
  loraQueued = false;
  LoRa.beginPacket();
  LoRa.print(loraMessage);
  LoRa.endPacket();
  pulseLed();
  net.publish("esp/test", "forwarded");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "fwd");
  display.drawString(0, 16, loraMessage);
  display.display();
}
