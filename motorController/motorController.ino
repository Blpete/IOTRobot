
/*
    Motor Controller for IOT ROBOT sketch for ESP8266
    Barry peterson
    Copyright 5/26/2018.  All Rights Reserved
*/


// Wifi
#include <ESP8266WiFi.h>

// Managing common includes
// I2C
#include <Wire.h>

// MicroController ID
#define DEVICE_ID ESP8266-MOTOR

// Wifi Settings
char ssid[] = "iotlink";     //  your network SSID (name)
char pass[] = "peterson1";  // your network password
int wifiStatus = WL_IDLE_STATUS;     // the Wifi radio's status
WiFiClient wifiClient;

// One time setup 
void setup()
{
    //Setup Serial Communications
    Serial.begin(115200);
}

// Main Looop 
void loop()
{
    scanI2CBus();

    logRealTimeClock();

    // OTA update loop
    ArduinoOTA.handle();

    // MQTT loop
    mqttClient.loop();

    delay(2000);
}


// Common Utilities
void logMessage(char* msg){
    Serial.println(msg);
}

void logPrint(char* msg) {
    Serial.print(msg);
}



void startupOTA() {
 // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  }