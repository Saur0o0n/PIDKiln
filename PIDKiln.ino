
/*
** PIDKiln v0.2 - high temperature kiln PID controller for ESP32
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
const char* ssid = "";  // Replace with your network credentials
const char* password = "";

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

const uint8_t WiFi_Tries=5;    // how many times (1 per second) tries to connect to wifi before failing

const int MAX_Prog_Size=10240;  // maximum file size (bytes) that can be uploaded as program, this limit is also defined in JS script (js/program.js)
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


void setup() {
  // Serial port for debugging purposes
  DBG Serial.begin(115200);

  // Setup function for LCD display from PIDKiln_LCD.ino
  setup_lcd();

  // Setup input devices
  setup_input();
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    DBG Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  DBG Serial.printf("Size of SSID %d",strlen(ssid));
  
  // Connect to Wi-Fi
  if(strlen(ssid)){
    bool wifi_failed=true;
    
    WiFi.begin(ssid, password);
    
    load_msg("connecting WiFi..");
    DBG Serial.println("Connecting to WiFi...");
    
    for(byte a=0; a<WiFi_Tries; a++){
      if (WiFi.status() == WL_CONNECTED){
        wifi_failed=false;
        continue;
      }
      delay(1000);
    }

    if(wifi_failed){  // if we failed to connect, stop trying
      WiFi.disconnect();
      load_msg("WiFi con. failed.");
      DBG Serial.println("WiFi connection failed");
    }else{
      DBG Serial.println(WiFi.localIP()); // Print ESP32 Local IP Address
      char lip[20];
      sprintf(lip," IP: %s",WiFi.localIP().toString().c_str());
      load_msg(lip);
      setup_webserver(); // Setup function for Webserver from PIDKiln_http.ino
    }
  }else load_msg("   -- Started! --");
  
  generate_index();
}

void loop() {
  input_loop();
}
