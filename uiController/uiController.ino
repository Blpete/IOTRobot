/*
    UI Controller for IOT ROBOT sketch for ESP8266
    Barry peterson
    Copyright 5/26/2018.  All Rights Reserved
*/


// Wifi
#include <ESP8266WiFi.h>


// Over the Air (OTA) updating
#include <ArduinoOTA.h>

// Managing common includes
// I2C
#include <Wire.h>

//  MQTT Interface
#include <PubSubClient.h>

#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library >


// MicroController ID
#define DEVICE_ID ESP8266-UI

// Wifi Settings
char ssid[] = "superduperlink ";     //  your network SSID (name)
char pass[] = "2603380139";  // your network password
int wifiStatus = WL_IDLE_STATUS;     // the Wifi radio's status
WiFiClient wifiClient;

LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address 

// MQTT configurationPubSubClient client(espClient);
// dependency on Wifi
PubSubClient mqttClient(wifiClient);
const char *mqttServer = "10.0.0.28";
const int mqttPort = 1883;
const char *mqttUser = "blpete";
const char *mqttPassword = "Peterson2016!";
const char *nodeName = "ESP8266-1";
boolean mqttInitialized = false;
boolean lcdInitialized = false;

// One time setup 
void setup()
{
    //Setup Serial Communications
    Serial.begin(115200);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0,0);
    lcd.print("It WORKS!!!!!!");

    scanI2CBus();

    
  // Configure Wifi
  configureWifi();

    // startup OTA
  startupOTA();
  
    // startup MQTT
  startupMQTT();


}

// Main Looop 
void loop()
{


    // OTA update loop
    ArduinoOTA.handle();

    // MQTT loop
    mqttClient.loop();

    delay(2000);
}

//  Configure Wifi subsystem
void configureWifi()
{
  WiFi.mode(WIFI_STA); // set to station mode

 // scanWifiNetworks();

  while (wifiStatus != WL_CONNECTED)
  {
    logMessage("Attempting to connect to WPA SSID: ");
    logMessage(ssid);
    // Connect to WPA/WPA2 network:
    wifiStatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(5000);
  }
}

void startupMQTT()
{
  // setup mqtt
  mqttClient.setServer(mqttServer, mqttPort);

  while (!mqttClient.connected())
  {
    Serial.println("Connecting to MQTT...");

    if (mqttClient.connect(nodeName, mqttUser, mqttPassword))
    {
      logMessage("MQTT connected");
    }
    else
    {
      logMessage("MQTT failed with state:");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }

//    const char* DEVICE = String(DEVICE_ID).c_str();
 // mqttClient.publish("devices", DEVICE);

  mqttClient.subscribe("esp/msg");
  mqttClient.setCallback(mqttCallback);
    mqttInitialized = true;
    logMessage("MQTT initialized");
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  payload[length] = '\0'; // Null terminator used to terminate the char array
  String message = (char *)payload;
  Serial.println(message);

  lcd.print(message);
}


// I2C common Utilities
void scanI2CBus()
{
  byte error, address;
  int nDevices;

  logMessage("Scanning I2C Bus...");

  nDevices = 0;
  for (address = 1; address < 127; address++)
  {

    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      logMessage("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
      //todo  logPrint(address,HEX);
      logMessage("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      logMessage("I2C Unknown error at address 0x");
      if (address < 16)
        logPrint("0");
      Serial.print(address, HEX);
    }
  }
  if (nDevices == 0)
    logMessage("No I2C devices found\n");
  else
    logMessage("I2C scan done\n");
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
