/*
** PIDKiln v1.1 - high temperature kiln PID controller for ESP32
**
** Copyright (C) 2019-2020 - Adrian Siemieniak
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************
**
** Some coding convention - used functions are in separate files, depending on what they do. So LCD stuff to LCD, HTTP/WWW to HTTP, rotary to INPUT etc. 
** All variables beeing global are written with capital leter on start of each word (or at least they should be). All definitions are all capital letters.
**
*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <FS.h>   // Include the SPIFFS library
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <soc/rtc_wdt.h>
#include <esp_task_wdt.h>

#include "PIDKiln.h"

/* 
** Static, editable parameters. Some of them, can be replaces with PIDKiln preferences.
** Please set them up before uploading.
*/
#define TEMPLATE_PLACEHOLDER '~' // THIS DOESN'T WORK NOW FROM HERE - replace it in library! Arduino/libraries/ESPAsyncWebServer/src/WebResponseImpl.h

#define DEBUG true
//#define DEBUG false


// Close cleanly file and delete file from SPIFFS
//
boolean delete_file(File &newFile){
char filename[32];
 if(newFile){
    strcpy(filename,newFile.name());
    DBG Serial.printf("[MAIN] Deleting uploaded file: \"%s\"\n",filename);
    newFile.flush();
    newFile.close();
    if(SPIFFS.remove(filename)){
      DBG Serial.println("[MAIN] Deleted!");
    }
    Generate_INDEX(); // Just in case user wanted to overwrite existing file
    return true;
 }else return false;
}


// Function check is uploaded file has only ASCII characters - this to be modified in future, perhaps to even narrowed down.
// Currently excluded are non printable characters, except new line and [] brackets. [] are excluded mostly for testing purpose.
// This way it should be faster then traversing char array
boolean check_valid_chars(byte a){
  if(a==0 || a==9 || a==95) return true; // end of file, tab, _
  if(a==10 || a==13) return true; // new line - Line Feed, Carriage Return
  if(a>=32 && a<=90) return true; // All basic characters, capital letters and numbers
  if(a>=97 && a<=122) return true; // small letters
  DBG Serial.print("[MAIN]  Faulty character ->");
  DBG Serial.print(a,DEC);
  DBG Serial.print(" ");
  DBG Serial.write(a);
  DBG Serial.println();
  return false;
}


// Check filename if it has only letters, numbers and . _ characters - other are not allowed for easy parsing and transfering
//
boolean valid_filename(char *file){
char c;
  while (c = *file++){
    if(strchr(allowed_chars_in_filename,c)) continue; // if every letter is on allowed list - we are good to go
    else return false;
  }
  return true;
}


// Main setup that invokes other subsetups to initialize other modules
//
void setup() {

// This should disable watchdog killing asynctcp and others - one of this should work :)
// This is not recommended, but if Webserver/AsyncTCP will hang (that has happen to me) - this will at least do not reset the device (and potentially ruin program).
// ESP32 will continue to work properly even in AsynTCP will hang - there will be no HTTP connection. If you do not like this - comment out next 6 lines.
  esp_task_wdt_init(1,false);
  esp_task_wdt_init(2,false);
  rtc_wdt_protect_off();
  rtc_wdt_disable();
  disableCore0WDT();
  disableCore1WDT();

  // Serial port for debugging purposes
  DBG Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    DBG Serial.println("[MAIN] An Error has occurred while mounting SPIFFS");
    return;
  }

  // Load all preferences
  Setup_Prefs();
  
  // Setup function for LCD display from PIDKiln_LCD.ino
  Setup_LCD();

  // Setup input devices
  Setup_Input();
  
  DBG Serial.printf("WiFi mode: %d, Retry count: %d, is wifi enabled: %d\n",Prefs[PRF_WIFI_MODE].value.uint8,Prefs[PRF_WIFI_RETRY_CNT].value.uint8,Prefs[PRF_WIFI_SSID].type);
  
  // Connect to WiFi if enabled
  if(Prefs[PRF_WIFI_MODE].value.uint8){ // If we want to have WiFi
    load_msg("connecting WiFi..");
    if(Setup_WiFi()){    // !!! Wifi connection FAILED
      DBG Serial.println("[MAIN] WiFi connection failed");
      load_msg(" WiFi con. failed");
    }else{
      IPAddress lips;
      
      Return_Current_IP(lips);
      DBG Serial.println(lips); // Print ESP32 Local IP Address
      char lip[20];
      sprintf(lip," IP: %s",lips.toString().c_str());
      load_msg(lip);
    }
  }else{
    // If we don't have Internet - assume there is no time set
    Setup_start_date(); // in PIDKiln_net
    Disable_WiFi();
    load_msg("   -- Started! --");
  }

  // (re)generate programs index file /programs/index.html
  Generate_INDEX();

  // Loads logs index
  Load_LOGS_Dir();
  
  // Clean logs on start - this will also call logs index generator
  Clean_LOGS();

  // Generate log index after cleanup
  Generate_LOGS_INDEX();
  
  // Setup program module
  Program_Setup();

  // Setup all sensors/relays
  Setup_Addons();

}


// Just a tiny loop - to be deleted ;)
//
void loop() {
  vTaskDelete(NULL);
}
