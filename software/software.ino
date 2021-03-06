
/*
 * Copyright 2022 by Michael Ammann (@mazgch)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ESP32 libraries:
//-----------------------------------
// Follow instruction on: https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
// Install Arduino -> Preferneces -> AddiionalBoard Manager URL, then in Board Manager add esp32 by EspressIf, 
// After that select Board u-blox NINA-W10 series and configure the target 4MB, 40Mhz, 
// Board Manager URL:    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
// Github Repository:    https://github.com/espressif/arduino-esp32 

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BluetoothSerial.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <SPI.h> 
#include <SD.h> 

// Third parties libraries
//-----------------------------------

// ArduinoMqttClient by Arduino
// Library Manager:    http://librarymanager/All#ArduinoMqttClient   
// Github Repository:  https://github.com/arduino-libraries/ArduinoMqttClient 
#include <ArduinoMqttClient.h>    

// ArduinoWebsockets by Gil Maimon
// Library Manager:    http://librarymanager/All#ArduinoWebsockets   
// Github Repository:  https://github.com/gilmaimon/ArduinoWebsockets
#include <ArduinoWebsockets.h>    

// ArduinoJson by Benoit Blanchon
// Library Manager:    http://librarymanager/All#ArduinoJson      
// Github Repository:  https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>          

// WiFiManager by Tapzu
// Library Manager:    http://librarymanager/All#tzapu,WiFiManager  
// Github Repository:  https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>         

// Sparkfun libraries
//-----------------------------------
 
// Library Manager:    http://librarymanager/All#SparkFun_u-blox_GNSS_Arduino_Library
// Github Repository:  https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>     

// Library Manager:    http://librarymanager/All#SparkFun_u-blox_SARA-R5_Arduino_Library
// Github Repository:  https://github.com/sparkfun/SparkFun_u-blox_SARA-R5_Arduino_Library
#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h>  

// Header files of this project
//-----------------------------------
#include "HW.h"
#include "LOG.h"
#include "CONFIG.h"
#include "UBXFILE.h"
#include "WLAN.h"
//#include "BLUETOOTH.h"
#include "GNSS.h"
#include "LBAND.h"
#include "LTE.h"

// ====================================================================================
// MAIN setup / loop
// ====================================================================================

void setup()
{
  // initialisation --------------------------------
  // serial port
  Serial.begin(115200, SERIAL_8N1, CDC_RX, CDC_TX);
  while (!Serial);
    /*nothing*/;
  //Log.init(LOG::LOG_LEVEL_DEBUG, &Serial);
  Log.info("-------------------------------------------------------------------");
  Config.init();
  Log.info("mazg.ch %s (%s)", Config.getDeviceTitle().c_str(), Config.getDeviceName().c_str());  
  // i2c wire
  UbxWire.begin(I2C_SDA, I2C_SCL); // Start I2C
  UbxWire.setClock(400000); //Increase I2C clock speed to 400kHz
  // SD card 
  UbxSd.init(); // handling SD card and files runs in a task
  //Bluetooth.begin();
  //Lte.enableDebugging(Serial);
  //Lte.enableAtDebugging(Serial); // we use UbxSerial for data logging instead
#ifdef WEBSOCKET_STREAM
  //Lte.enableAtDebugging(Websocket); // forward all messages
#endif  
  Lte.init();  // LTE runs in a task
  Wlan.init(); // WLAN runs in a tasks, creates an additional LED task 
  //LBand.enableDebugging()
#ifdef WEBSOCKET_STREAM
  //LBand.setOutputPort(Websocket); // forward all messages
#endif  
  if (!LBand.detect()) {
    Log.warning("LBAND NEO-D9S not detected, check wiring");
  }
  //Gnss.enableDebugging();
#ifdef WEBSOCKET_STREAM
  Gnss.setOutputPort(Websocket); // forward all messages
#endif  
  if (!Gnss.detect()) { 
    Log.warning("GNSS ZED-F9 not detected, check wiring");
  }
}

void loop()
{
  LBand.poll();
  Gnss.poll();
  //Bluetooth.poll();
  Log.poll();
  delay(50);
}
