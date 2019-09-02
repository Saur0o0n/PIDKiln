/*
** Network related functions
**
*/

// Tries to connect to wifi
//
boolean start_wifi(){
bool wifi_failed=true;

  if(!strlen(ssid)) return 1;
    
  WiFi.begin(ssid, password);
  DBG Serial.println("Connecting to WiFi...");
    
  for(byte a=0; a<WiFi_Tries; a++){
    delay(1000);
    if (WiFi.status() == WL_CONNECTED){
      wifi_failed=false;
      return 0;
    }
  }
  
  WiFi.disconnect();
  return 1;
}

// Starts WiFi and all networks services
//
boolean setup_wifi(){

  if(strlen(ssid)){
    if(start_wifi()) return 1;  // if we failed to connect, stop trying
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // configure RTC clock with NTP server - ot at least try
    setup_webserver(); // Setup function for Webserver from PIDKiln_http.ino
    return 0;
  }else return 1;
}

// Prints local time
//
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
