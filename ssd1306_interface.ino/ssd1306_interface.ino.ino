// SPDX-License-Identifier: BSD-3-Clause
//
// Part example, not part of the robot. Modified from Adafruit's
// SSD1306 128x32 I2C example. The Adafruit BSD text above the original
// example, and the license.txt in this folder, cover that code.
// Project files elsewhere in this repository are GPL-3.0-or-later.

/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x32 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be included in any redistribution.
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define OLED_RESET LED_BUILTIN
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int counter = 0;
bool displayOk = false;

void eeWriteInt(int pos, int val) {
  byte* p = (byte*)&val;
  EEPROM.write(pos, *p);
  EEPROM.write(pos + 1, *(p + 1));
  EEPROM.write(pos + 2, *(p + 2));
  EEPROM.write(pos + 3, *(p + 3));
  EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*)&val;
  *p = EEPROM.read(pos);
  *(p + 1) = EEPROM.read(pos + 1);
  *(p + 2) = EEPROM.read(pos + 2);
  *(p + 3) = EEPROM.read(pos + 3);
  return val;
}

void scanI2CBus() {
  int nDevices = 0;
  Serial.println("Scanning I2C Bus...");
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.println("I2C scan done");
  }
}

void testdrawchar(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  for (int16_t i = 0; i < 256; i++) {
    if (i == '\n') {
      display.write(' ');
    } else {
      display.write(i);
    }
  }
  display.display();
  delay(2000);
}

void testdrawstyles(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Hello, world!"));
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.println(3.141592);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  char buf[40];
  snprintf(buf, sizeof(buf), "%4d.00", counter);
  display.print(buf);
  display.println(0xDEADBEEF, HEX);
  display.display();
  delay(2000);
}

void testscrolltext(void) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  char buf[40];
  snprintf(buf, sizeof(buf), "Evans $ %4d.00", counter);
  display.println(buf);
  counter++;
  // Commit every 20 ticks. A commit on every frame wears out the flash sector.
  if ((counter % 20) == 0) {
    eeWriteInt(0, counter);
  }
  display.display();
  delay(100);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  scanI2CBus();

  EEPROM.begin(512);
  int eesetup = eeGetInt(8);
  if (eesetup != 100) {
    eeWriteInt(8, 100);
    eeWriteInt(0, counter);
  } else {
    counter = eeGetInt(0);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    return;
  }
  displayOk = true;
  display.clearDisplay();
  testscrolltext();
}

void loop() {
  if (!displayOk) {
    delay(1000);
    return;
  }
  testscrolltext();
  delay(500);
}
