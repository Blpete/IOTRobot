//#include <WiFiClient.h>
//#include <WiFiServer.h>
#include <WiFi.h>
//#include <WiFiUdp.h>

#include <PubSubClient.h>

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`

#include <SPI.h>
#include <LoRa.h>
#include<Arduino.h>


//OLED pins to ESP32 GPIOs via this connecthin:
//OLED_SDA — GPIO4
//OLED_SCL — GPIO15
//OLED_RST — GPIO16

SSD1306 display(0x3c, 4, 15);

 char ssid[] = "iotlink";     //  your network SSID (name)
char pass[] = "2603380139!";  // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status


// WIFI_LoRa_32 ports

// GPIO5 — SX1278’s SCK
// GPIO19 — SX1278’s MISO
// GPIO27 — SX1278’s MOSI
// GPIO18 — SX1278’s CS
// GPIO14 — SX1278’s RESET
// GPIO26 — SX1278’s IRQ(Interrupt Request)

#define SS 18
#define RST 14
#define DI0 26
// #define BAND 429E6 //915E6

// #define BAND 434500000.00
#define BAND 915E6

#define spreadingFactor 9
// #define SignalBandwidth 62.5E3
#define SignalBandwidth 31.25E3
#define preambleLength 8
#define codingRateDenominator 8

//create instance of pubsubclient
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqttServer = "10.0.0.28";
const int mqttPort = 1883;
const char* mqttUser = "blpete";
const char* mqttPassword = "Peterson2016!";


int counter = 0;

void setup() {
  pinMode(25,OUTPUT); //Send success, LED will bright 1 second
  
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW); // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH);
  
  Serial.begin(115200);
  while (!Serial); //If just the the basic function, must connect to a computer
   // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(5000);
  }
  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  printCurrentNet();
  printWifiData();
  
  // setup mqtt
  client.setServer(mqttServer, mqttPort);
 
  while (!client.connected()) {
      Serial.println("Connecting to MQTT...");
   
      if (client.connect("ESP32Client", mqttUser, mqttPassword )) {
   
        Serial.println("connected");
   
      } else {
   
        Serial.print("failed with state ");
        Serial.print(client.state());
        delay(2000);
   
      }
  }
  client.publish("esp/test", "Hello from ESP32");
  client.subscribe("esp/msg");
    client.setCallback(callback);


// Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(5,5,"LoRa Sender");
  display.display();
  
  SPI.begin(5,19,27,18);
  LoRa.setPins(SS,RST,DI0);
  Serial.println("LoRa Sender");
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
  Serial.print("LoRa Spreading Factor: ");
  Serial.println(spreadingFactor);
  LoRa.setSpreadingFactor(spreadingFactor);
  
  Serial.print("LoRa Signal Bandwidth: ");
  Serial.println(SignalBandwidth);
  LoRa.setSignalBandwidth(SignalBandwidth);

  LoRa.setCodingRate4(codingRateDenominator);

  LoRa.setPreambleLength(preambleLength);
  
  Serial.println("LoRa Initial OK!");
  display.drawString(5,20,"LoRa Initializing OK!");
  display.display();
  delay(2000);
}

void loop() {
  Serial.print("Sending packet: ");
  Serial.println(counter);
  
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(3, 5, "Sending packet ");
  display.drawString(50, 30, String(counter));
  display.display();
  
  // send packet
  LoRa.beginPacket();
  LoRa.print("Hello..");
  LoRa.print(counter);
  LoRa.endPacket();
  
  client.publish("esp/test", "Sending packet");
  counter++;
  digitalWrite(25, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(1000); // wait for a second
  digitalWrite(25, LOW); // turn the LED off by making the voltage LOW
  delay(1000); // wait for a second

  client.loop();
// delay(3000);
}


void printWifiData() {
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  Serial.print(mac[5], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.println(mac[0], HEX);

}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

//  // print the MAC address of the router you're attached to:
//  byte bssid[6];
//  WiFi.BSSID(bssid);
//  Serial.print("BSSID: ");
//  Serial.print(bssid[5], HEX);
//  Serial.print(":");
//  Serial.print(bssid[4], HEX);
//  Serial.print(":");
//  Serial.print(bssid[3], HEX);
//  Serial.print(":");
//  Serial.print(bssid[2], HEX);
//  Serial.print(":");
//  Serial.print(bssid[1], HEX);
//  Serial.print(":");
//  Serial.println(bssid[0], HEX);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
//  byte encryption = WiFi.encryptionType();
//  Serial.print("Encryption Type:");
//  Serial.println(encryption, HEX);
//  Serial.println();
}

void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
    String s = String((char*) payload);
   display.drawString(55, 10, "rec:");
     display.display();
     char stupid[50];
     s.toCharArray(stupid, 50);
   client.publish("esp/test", stupid);
   Serial.print("Msg:");
   Serial.print(s);

  // send packet
  LoRa.beginPacket();
  LoRa.print("Hello..");
  LoRa.print(stupid);
  LoRa.endPacket();
  
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}



