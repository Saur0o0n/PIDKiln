
/*
** PIDKiln v0.3 - high temperature kiln PID controller for ESP32
**
** (c) 2019 - Adrian Siemieniak
**
** Some coding convention - used functions are in separate files, depending on what they do. So LCD stuff to LCD, HTTP/WWW to HTTP, rotary to INPUT etc. 
** All variables beeing global are written with capital leter on start of each word. All definitions are capital letters.
*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <FS.h>   // Include the SPIFFS library
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include "PIDKiln.h"

/* 
** Static, editable parameters. Some of them, can be replaces with PIDKiln preferences.
** Please set them up before uploading.
*/
#define TEMPLATE_PLACEHOLDER '~' // THIS DOESN'T WORK NOW FROM HERE - replace it in library! Arduino/libraries/ESPAsyncWebServer/src/WebResponseImpl.h

#define DEBUG true
//#define DEBUG false
/* 
** Some definitions - usually you should not edit this, but you may want to
*/
#define ENCODER0_PINA    35
#define ENCODER0_PINB    34
#define ENCODER0_BUTTON  32
#define ENCODER_BUTTON_DELAY 150  // 150ms between button press readout
#define ENCODER_ROTATE_DELAY 120  // 120ms between rotate readout
const uint16_t Long_Press=400; // long press button takes about 0,9 second

const int MAX_Prog_File_Size=10240;  // maximum file size (bytes) that can be uploaded as program, this limit is also defined in JS script (js/program.js)
const int MAX_Temp=1350;        // maximum temperature for kiln/programs

// Other variables
//



// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


// Close cleanly file and delete file from SPIFFS
//
boolean delete_file(File &newFile){
char filename[32];
 if(newFile){
    strcpy(filename,newFile.name());
    DBG Serial.printf("Deleting uploaded file: \"%s\"\n",filename);
    newFile.flush();
    newFile.close();
    if(SPIFFS.remove(filename)){
      DBG Serial.println("Deleted!");
    }
    generate_index(); // Just in case user wanted to overwrite existing file
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
  DBG Serial.print("  Faulty character ->");
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
  // Serial port for debugging purposes
  DBG Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    DBG Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Load all preferences
  setup_prefs();
  
  // Setup function for LCD display from PIDKiln_LCD.ino
  setup_lcd();

  // Setup input devices
  setup_input();
  
  DBG Serial.printf("WiFi mode: %d, Retry count: %d, is wifi enabled: %d\n",Prefs[PRF_WIFI_RETRY_CNT].value.uint8,Prefs[PRF_WIFI_RETRY_CNT].value.uint8,Prefs[PRF_WIFI_SSID].type);
  
  // Connect to Wi-Fi
  if(Prefs[PRF_WIFI_SSID].type){
    load_msg("connecting WiFi..");
    if(setup_wifi()){    // !!! Wifi connection FAILED
      DBG Serial.println("WiFi connection failed");
      load_msg(" WiFi con. failed");
    }else{
      DBG Serial.println(WiFi.localIP()); // Print ESP32 Local IP Address
      char lip[20];
      sprintf(lip," IP: %s",WiFi.localIP().toString().c_str());
      load_msg(lip);
    }
  }else{
    // If we don't have Internet - assume there is no time set
    setup_start_date(); // in PIDKiln_net
    load_msg("   -- Started! --");
  }
  
  generate_index();

  // Setup program module
  program_setup();
}

void loop() {
  input_loop();
  program_loop();
}
