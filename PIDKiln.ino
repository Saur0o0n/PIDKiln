
/*
** PIDKiln v0.2 - high temperature kiln PID controller for ESP32
**
** (c) 2019 - Adrian Siemieniak
** 
*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <FS.h>   // Include the SPIFFS library
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

/* 
** Static, editable parameters. Some of them, can be replaces with PIDKiln preferences.
** Please set them up before uploading.
*/
const int max_prog_size=10240;  // maximum file size (bytes) that can be uploaded as program

const char* ssid = "";  // Replace with your network credentials
const char* password = "";

#define TEMPLATE_PLACEHOLDER '~' // THIS DOESN'T WORK NOW - replace it in library! Arduino/libraries/ESPAsyncWebServer/src/WebResponseImpl.h

#define encoder0PinA    35
#define encoder0PinB    34
#define encoder0Button  32
#define ENCODER_BUTTON_DELAY 180  // 200ms between button press readout
#define ENCODER_ROTATE_DELAY 120  // 130ms between rotate readout
const unsigned int long_press=1500; // long press button takes 1,5second

/* 
** Some definitions - you should not edit this - except DEBUG if you wish 
*/
#define DEBUG true

#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
#define DBG if(DEBUG)

#define FORMAT_SPIFFS_IF_FAILED true

const char* pver = "PIDKiln v0.2";
const char* pdate = "2019.08.22";

// Other variables
//
String template_str;  // Stores template pareser output


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


// Close cleanly file and delete file from SPIFFS
//
boolean delete_file(File &newFile){
 if(newFile){
    char ntmp[sizeof(newFile.name())+1];
    strcpy(ntmp,newFile.name());
    newFile.flush();
    DBG Serial.printf("Deleting uploaded file: \"%s\"\n",ntmp);
    newFile.close();
    if(SPIFFS.remove(ntmp)){
      DBG Serial.println("Deleted!");
    }
    generate_index(); // Just in case user wanted to overwrite existing file
    return true;
 }else return false;
}

// Function check is uploaded file has only ASCII characters - this to be modified in future, perhaps to even narrow down.
// Currently excluded are non printable characters, except new line and []. [] is excluded mostly for testing purpose.
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


  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    load_msg("connecting WiFi..");
    DBG Serial.println("Connecting to WiFi...");
  }

  // Print ESP32 Local IP Address
  DBG Serial.println(WiFi.localIP());
  char lip[20];
  sprintf(lip," IP: %s",WiFi.localIP().toString().c_str());
  load_msg(lip);
  // Setup function for Webserver from PIDKiln_http.ino
  setup_webserver();

  generate_index();
}

void loop() {
  input_loop();
}
