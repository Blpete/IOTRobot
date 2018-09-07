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
//#include <RTC_DS1307.h>

// Over the Air (OTA) updating
#include <ArduinoOTA.h>

//  MQTT Interface
#include <PubSubClient.h>

#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library >

#include <EEPROM.h>


//
// Configuration Data
//

// MicroController ID
int DEVICE_ID = 1;
char DEVICE[12];

// Wifi Settings
char ssid[] = "iotlink";         //  your network SSID (name)
char pass[] = "2603380139!";     //"peterson1";  // your network password
int wifiStatus = WL_IDLE_STATUS; // the Wifi radio's status
WiFiClient wifiClient;

// Real Time Clock
//RTC_DS1307 rtc;

// MQTT configurationPubSubClient client(espClient);
// dependency on Wifi
PubSubClient mqttClient(wifiClient);
const char *mqttServer = "10.0.0.28";
const int mqttPort = 1883;
const char *mqttUser = "blpete";
const char *mqttPassword = "Peterson2016!";
const char *nodeName = "ESP8266-1";
boolean mqttInitialized = false;

LiquidCrystal_I2C lcd(0x3F, 16, 2);

// One time setup
void setup()
{
  DEVICE_ID = ESP.getChipId();
  sprintf(DEVICE,"\t%08X", DEVICE_ID);
  //const char* DEVICE = String(DEVICE_ID).c_str();
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
  logMessage("Device Initialized:");
  logMessage(DEVICE);

  EEPROM.begin(256);
  
  lcd.begin(16, 2);
  lcd.init();      // initializing the LCD
  lcd.backlight(); // Enable or Turn On the backlight
  lcd.setCursor(0, 1);
  lcd.print("I'm Alive"); // Start Print text to Line 1
  lcd.setCursor(0, 2);
  lcd.print(DEVICE); // Start Print Test to Line 2
                                        // lcdInitialized = true;
                                     
 // readConfig();
 
                                        
                                        
}

// Main Looop
void loop()
{
  //scanI2CBus();

  //   logRealTimeClock();

  // OTA update loop
  ArduinoOTA.handle();

  // MQTT loop
  mqttClient.loop();

  delay(2000);
}


// Common Utilities
void logMessage(const char *msg)
{
  Serial.println(msg);
  if (mqttInitialized)
  {
    mqttClient.publish("esp/log", msg);
  }
  lcd.setCursor(0, 1);
  lcd.print(msg);
}

void logPrint(char *msg)
{
  Serial.print(msg);
}

void readConfig() {
  
  byte high = EEPROM.read(0); //read the first half
  byte low = EEPROM.read(1); //read the second half
  
  int size = (high << 8) + low; //reconstruct the integer

    char str[80];
   sprintf(str, "Value of Len = %d", size);
   logMessage(str);
   
  char temp[size];// = new char[size];
  for (int i=0; i<size; i++) {
    temp[i] = EEPROM.read(i+2);
  }
  temp[size]=0;
  logMessage("Reading CONFIG from EEPROM");
  logMessage(temp);
}

void writeConfig(String config) {
  int len =  config.length();
  byte high =len/256;
  byte low =len- (high*256);

    char str[80];
   sprintf(str, "Value of Len = %d", low);
   logMessage(str);
  
  EEPROM.write(0, high);
  EEPROM.write(1,  low);
  for (int i=0; i<len; i++) {
    EEPROM.write(i+2,config[i]);
  }
  EEPROM.commit();
  logMessage("wrote config");
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

//  Configure Wifi subsystem
void configureWifi()
{
  WiFi.mode(WIFI_STA); // set to station mode

  scanWifiNetworks();

  while (wifiStatus != WL_CONNECTED)
  {
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
void printWifiData()
{
  // print the SSID of the network you're attached to:
  logMessage("SSID: ");
  // logMessage(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  logPrint("IP Address: ");
  Serial.println(ip);
  lcd.setCursor(0, 1);
  lcd.print(ip);
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

void scanWifiNetworks()
{
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
  {
    Serial.println("no networks found");
  }
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
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

void startupRTC()
{
  setRealTimeClock();
}

void setRealTimeClock()
{
  logMessage("RTC is being set!");
  // following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(__DATE__, __TIME__));
  // rtc.SetTime(00,40,13,1,06,03,17);
}

void logRealTimeClock()
{
  // rtc.DisplayTime();
  long now = 0; //rtc.now();
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

void startupOTA()
{
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
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
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
  mqttInitialized = true;
//    const char* DEVICE = String(DEVICE_ID).c_str();
  mqttClient.publish("devices", DEVICE);

  mqttClient.subscribe("esp/msg");
  mqttClient.setCallback(mqttCallback);
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

  executeCommand(message);
}


 String getValue(String data, char separator, int index)
{
//    int found = 0;
//    int strIndex[] = { 0, -1 };
//    int maxIndex = data.length() - 1;
//
//    for (int i = 0; i <= maxIndex && found <= index; i++) {
//        if (data.charAt(i) == separator || i == maxIndex) {
//            found++;
//            strIndex[0] = strIndex[1] + 1;
//            strIndex[1] = (i == maxIndex) ? i+1 : i;
//        }
//    }
//    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
    String val ="";
    int i = data.indexOf(separator);
    if (i >0) {
      val = data.substring(i+1);
    }
    return val;
}


void executeCommand(String message)
{
  String param = getValue(message, '=',0);
  if (message == "lcd-off")
  {
    Serial.println("off");
    lcd.noBacklight();
  }
  else if (message == "lcd-on")
  {
    Serial.println("on");
    lcd.backlight();
  } else if (message.startsWith("lcd-write0")) {
   lcd.setCursor(0, 0);
    lcd.print(param);
  }  else if (message.startsWith("lcd-write1")) {
   lcd.setCursor(0, 1);
    lcd.print(param);
  }
  else if (message == "lcd-clear") {
    lcd.clear();
  }
  else if (message == "pin1-on")
  {
    digitalWrite(BUILTIN_LED, LOW); // Turn the LED on (Note that LOW is the voltage level
  }
  else if (message == "pin1-off")
  {
    digitalWrite(BUILTIN_LED, HIGH); // Turn the LED off by making the voltage HIGH
  }
  else if (message == "i2c-scan")
  {
    scanI2CBus();
  }
  else if (message == "wifi-scan")
  {
    scanWifiNetworks();
  } else if (message.startsWith("write-config")) {
    writeConfig(param);
  }
  else if (message == "read-config"){
    readConfig();
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    // lcd.autoscroll();
    lcd.print(message);
  }

}
