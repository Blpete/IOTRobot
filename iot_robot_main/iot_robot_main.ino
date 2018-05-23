
/*
    Master IOT ROBOT sketch for ESP8266
    Barry peterson
    Copyright 5/26/2018.  All Rights Reserved
*/

// Wifi
#include <ESP8266WiFi.h>


// Managing common includes
// I2C
#include <Wire.h>
//
//#include <WiFiClient.h>
//#include <WiFi.h>
//#include <WiFiUdp.h>
//#include <WiFiServer.h>

// Real Time Clock (RTC)
#include <RTC_DS1307.h>

// Over the Air (OTA) updating
#include <ArduinoOTA.h>

//  MQTT Interface
#include <PubSubClient.h>

//
// Configuration Data
//

// MicroController ID
#define DEVICE_ID ESP8266-1

// Wifi Settings
char ssid[] = "iotlink";     //  your network SSID (name)
char pass[] = "peterson1";  // your network password
int wifiStatus = WL_IDLE_STATUS;     // the Wifi radio's status
WiFiClient wifiClient;

// Real Time Clock
RTC_DS1307 rtc;

// MQTT configurationPubSubClient client(espClient);
// dependency on Wifi
PubSubClient mqttClient(wifiClient);
const char* mqttServer = "10.0.0.28";
const int mqttPort = 1883;
const char* mqttUser = "blpete";
const char* mqttPassword = "Peterson2016!";

// One time setup 
void setup()
{
    //Setup Serial Communications
    Serial.begin(115200);
    logMessage("Serial Interface started at 115200 baud");

    // Setup I2C communications
    Wire.begin();
    logMessage("I2C communicaton started");

    // Configure Wifi
    configureWifi();

    // Real Time Clock
    startupRTC();

    // startup OTA
    startupOTA();

    // startup MQTT
    startupMQTT();

    // Configure Device
    logMessage("Device Initialized:"+ DEVICE_ID);
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


// I2C common Utilities
void scanI2CBus() {
  byte error, address;
  int nDevices;
 
  logMessage("Scanning I2C Bus...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  {
 
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      logMessage("I2C device found at address 0x");
      if (address<16) 
        Serial.print("0");
        Serial.println(address,HEX);
    //todo  logPrint(address,HEX);
     logMessage("  !");
 
      nDevices++;
    }
    else if (error==4) 
    {
      logMessage("I2C Unknown error at address 0x");
      if (address<16) 
        logPrint("0");
        Serial.print(address,HEX);
    }    
  }
  if (nDevices == 0)
    logMessage("No I2C devices found\n");
  else
    logMessage("I2C scan done\n");
}


//  Configure Wifi subsystem
void configureWifi()
{
    WiFi.mode(WIFI_STA); // set to station mode

    scanWifiNetworks();
    
    while (wifiStatus != WL_CONNECTED) {
      logMessage("Attempting to connect to WPA SSID: ");
      logMessage(ssid);
      // Connect to WPA/WPA2 network:
      wifiStatus = WiFi.begin(ssid, pass);

      // wait 10 seconds for connection:
      delay(5000);
    }
    printWifiData();
}

// Log WIFI configuration Data
void printWifiData() {
  // print the SSID of the network you're attached to:
  logMessage("SSID: ");
 // logMessage(WiFi.SSID());
 

    // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  logPrint("IP Address: ");
  Serial.println(ip);
 // logMessage(ip);


  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  logPrint("MAC address: ");
//  logPrint(mac[5], HEX);
//  logPrint(":");
//  logPrint(mac[4], HEX);
//  logPrint(":");
//  logPrint(mac[3], HEX);
//  logPrint(":");
//  logPrint(mac[2], HEX);
//  logPrint(":");
//  logPrint(mac[1], HEX);
//  logPrint(":");
//  logPrint(mac[0], HEX);

  // Wifi signal strength
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);
}

void scanWifiNetworks() {
 // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
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
  Serial.println("");
}


void startupRTC() {
  setRealTimeClock();
}

void setRealTimeClock(){
   logMessage("RTC is being set!");
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(__DATE__, __TIME__));
    rtc.SetTime(00,40,13,1,06,03,17);
}

void logRealTimeClock () {
    rtc.DisplayTime();
    long now = rtc.UnixTimestamp();
    logMessage("RTC value:");
    Serial.println(now);
//    Serial.print(now.year(), DEC);
//    Serial.print('/');
//    Serial.print(now.month(), DEC);
//    Serial.print('/');
//    Serial.print(now.day(), DEC);
//    Serial.print(' ');
//    Serial.print(now.hour(), DEC);
//    Serial.print(':');
//    Serial.print(now.minute(), DEC);
//    Serial.print(':');
//    Serial.print(now.second(), DEC);
//    Serial.println();
//    
//    Serial.print(" since midnight 1/1/1970 = ");
//    Serial.print(now.unixtime());
//    Serial.print("s = ");
//    Serial.print(now.unixtime() / 86400L);
//    Serial.println("d");
//    
//      Serial.println();

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


void startupMQTT() {
    // setup mqtt
  mqttClient.setServer(mqttServer, mqttPort);
 
  while (!mqttClient.connected()) {
      Serial.println("Connecting to MQTT...");
   
      if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword )) {
        logMessage("MQTT connected");
      } else {
        logMessage("MQTT failed with state:");
        Serial.print(mqttClient.state());
        delay(2000);
   
      }
  }
  mqttClient.publish("esp/test", "Hello from ESP32");
  mqttClient.subscribe("esp/msg");
  mqttClient.setCallback(mqttCallback);
}



void mqttCallback(char* topic, byte* payload, unsigned int length) {
  
    logMessage("MQTT Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
    const char * stupid ="stupid";
    mqttClient.publish("esp/test", stupid);
    Serial.print("Msg:");
    Serial.print(stupid);

  // // Switch on the LED if an 1 was received as first character
  // if ((char)payload[0] == '1') {
  //   digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
  //   // but actually the LED is on; this is because
  //   // it is acive low on the ESP-01)
  // } else {
  //   digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  // }

}
