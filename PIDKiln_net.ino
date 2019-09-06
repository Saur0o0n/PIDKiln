/*
** Network related functions
**
*/

// Tries to connect to wifi
//
boolean start_wifi(){
bool wifi_failed=true;

  if(!strlen(Prefs[PRF_WIFI_SSID].value.str)) return 1;
    
  WiFi.begin(Prefs[PRF_WIFI_SSID].value.str, Prefs[PRF_WIFI_PASS].value.str);
  DBG Serial.println("Connecting to WiFi...");
    
  for(byte a=0; a<Prefs[PRF_WIFI_RETRY_CNT].value.uint8; a++){
    delay(1000);
    if (WiFi.status() == WL_CONNECTED){
      wifi_failed=false;
      return 0;
    }
  }
  
  WiFi.disconnect();
  return 1;
}


// Set some default time - if there is no NTP
//
void setup_start_date(){
struct timeval tv;
struct tm mytm;

  mytm.tm_hour = 0;       //0-23
  mytm.tm_min = 0;        //0-59
  mytm.tm_sec = 0;        //0-59
  mytm.tm_mday = 1;       //1-31 - depending on month
  mytm.tm_mon = 1;        //1-12
  mytm.tm_year = 2019-1900;  // year after 1900
  time_t t = mktime(&mytm);

  tv.tv_sec = t;
  tv.tv_usec = 0;

  settimeofday(&tv, NULL);
}

// Starts WiFi and all networks services
//
boolean setup_wifi(){
struct tm timeinfo;

  if(strlen(Prefs[PRF_WIFI_SSID].value.str)){
    if(start_wifi()) return 1;  // if we failed to connect, stop trying
    
    configTime(Prefs[PRF_GMT_OFFSET].value.uint16, Prefs[PRF_DAYLIGHT_OFFSET].value.uint16, Prefs[PRF_NTPSERVER1].value.str, Prefs[PRF_NTPSERVER2].value.str, Prefs[PRF_NTPSERVER3].value.str); // configure RTC clock with NTP server - ot at least try
    if(!getLocalTime(&timeinfo)) setup_start_date();    // if failed to setup NTP time - start default clock
    
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
