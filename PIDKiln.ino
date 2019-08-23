
/*
** PIDKiln v0.1 - high temperature kiln PID controller for ESP32
**
** (c) 2019 - Adrian Siemieniak
** 
**/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Regexp.h>
#include <FS.h>   // Include the SPIFFS library

#include "ESPAsyncWebServer.h"

// Some definitions - you should not edit this - except DEBUG if you wish\
//
#define DEBUG true

#define PRG_DIRECTORY "/programs/"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
#define DBG if(DEBUG)
const char* pver = "Pidklin 0.1 - 2019.08.22";

// Static, editable parameters
//
const int max_prog_size=10240;  // maximum file size (bytes) that can be uploaded as program

const char* ssid = "Your WiFi SID";  // Replace with your network credentials
const char* password = "Your WiFi Password";


// Other variables
//
String template_str;  // Stores template pareser output


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


// Close cleanly file and delete file from SPIFFS
//
boolean delete_file(File &newFile){
 if(newFile){
    char ntmp[sizeof(newFile.fullName())+1];
    strcpy(ntmp,newFile.fullName());
    newFile.flush();
    DBG Serial.printf("Deleting uploaded file: \"%s\"\n",ntmp);
    newFile.close();
    if(SPIFFS.remove(ntmp)){
      DBG Serial.println("Deleted!");
    }
    generate_index(); // Just in case user wanted to overwrite exsisting file
    return true;
 }else return false;
}

// Function check is uploaded file has only ASCII characters - this to be modified in future, perhaps to even narrow down.
// Currently excluded are non printable chaaracters, except new line and []. [] is excluded mostly for testing purpose.
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

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  DBG Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Serve some static data
  server.serveStatic("/icons/", SPIFFS, "/icons/");
  server.serveStatic("/js/", SPIFFS, "/js/");
  server.serveStatic("/css/", SPIFFS, "/css/");

  // Set default file for programs to index.html - because webserver was programed by... :/
  server.serveStatic("/programs/", SPIFFS, "/programs/").setDefaultFile("index.html");

  // Upload new programs
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, handleUpload);

  server.on("/debug_board.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/debug_board.html", String(), false, debug_board);
  });
/*
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });
*/
/*  server.on("/programs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/programs/");
  });
*/


  // Start server
  server.begin();

  generate_index();
}

void loop() {

}
