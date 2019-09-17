/*
** Network related functions
**
*/

// Tries to connect to wifi
//
boolean start_wifi(){
bool wifi_failed=true;

  if(!strlen(Prefs[PRF_WIFI_SSID].value.str)) return 1;   // if there is no SSID
    
  WiFi.begin(Prefs[PRF_WIFI_SSID].value.str, Prefs[PRF_WIFI_PASS].value.str);
  DBG Serial.println("Connecting to WiFi...");
    
  for(byte a=0; !Prefs[PRF_WIFI_RETRY_CNT].value.uint8 || a<Prefs[PRF_WIFI_RETRY_CNT].value.uint8; a++){  // if PRF_WIFI_RETRY_CNT - try indefinitely
    delay(1000);
    if (WiFi.status() == WL_CONNECTED){
      wifi_failed=false;
      return 0;
    }
    DBG Serial.println("Connecting to WiFi...");
  }
  
  WiFi.disconnect();
  return 1;
}


void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


// Set some default time - if there is no NTP
//
void Setup_start_date(){
struct timeval tv;
struct tm mytm;
uint8_t pos;
char *tmp,msg[20];

  strcpy(msg,Prefs[PRF_INIT_DATE].value.str);
  tmp=strtok(msg,".-:");
  DBG Serial.printf("Y:%s ",tmp);
  mytm.tm_year = atoi(tmp)-1900;  // year after 1900
  
  tmp=strtok(NULL,".-:");
  DBG Serial.printf("M:%s ",tmp);
  mytm.tm_mon = atoi(tmp)-1;        //0-11 WHY???
  
  tmp=strtok(NULL,".-:");
  DBG Serial.printf("D:%s ",tmp);
  mytm.tm_mday = atoi(tmp);       //1-31 - depending on month

  strcpy(msg,Prefs[PRF_INIT_TIME].value.str);
  tmp=strtok(msg,".-:");
  DBG Serial.printf("\tH:%s ",tmp);
  mytm.tm_hour = atoi(tmp);       //0-23

  tmp=strtok(NULL,".-:");
  DBG Serial.printf("m:%s ",tmp);
  mytm.tm_min = atoi(tmp);        //0-59

  tmp=strtok(NULL,".-:");
  DBG Serial.printf("s:%s\n",tmp);
  mytm.tm_sec = atoi(tmp);        //0-59
  
  time_t t = mktime(&mytm);

  tv.tv_sec = t;
  tv.tv_usec = 0;

  settimeofday(&tv, NULL);
  
  printLocalTime();
}


// Starts WiFi and all networks services
//
boolean setup_wifi(){
struct tm timeinfo;
Setup_start_date();
  if(strlen(Prefs[PRF_WIFI_SSID].value.str)){
    if(start_wifi()){
      Setup_start_date();
      return 1;  // if we failed to connect, stop trying
    }
    
    configTime(Prefs[PRF_GMT_OFFSET].value.uint16, Prefs[PRF_DAYLIGHT_OFFSET].value.uint16, Prefs[PRF_NTPSERVER1].value.str, Prefs[PRF_NTPSERVER2].value.str, Prefs[PRF_NTPSERVER3].value.str); // configure RTC clock with NTP server - ot at least try
    if(!getLocalTime(&timeinfo)) Setup_start_date();    // if failed to setup NTP time - start default clock

    printLocalTime();
      
    setup_webserver(); // Setup function for Webserver from PIDKiln_http.ino
    return 0;
  }else return 1;
}
