/*
** Network related functions
**
*/

#define DEFAULT_AP "PIDKiln_AP"
#define DEFAULT_PASS "hotashell"

// Other variables
//

// Print local time to serial
void printLocalTime(){
struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("[NET] Failed to obtain time");
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
  DBG dbgLog(LOG_INFO,"[NET] Y:%s ",tmp);
  mytm.tm_year = atoi(tmp)-1900;  // year after 1900
  
  tmp=strtok(NULL,".-:");
  DBG dbgLog(LOG_INFO,"[NET] M:%s ",tmp);
  mytm.tm_mon = atoi(tmp)-1;        //0-11 WHY???
  
  tmp=strtok(NULL,".-:");
  DBG dbgLog(LOG_INFO,"[NET] D:%s ",tmp);
  mytm.tm_mday = atoi(tmp);       //1-31 - depending on month

  strcpy(msg,Prefs[PRF_INIT_TIME].value.str);
  tmp=strtok(msg,".-:");
  DBG dbgLog(LOG_INFO,"[NET] H:%s ",tmp);
  mytm.tm_hour = atoi(tmp);       //0-23

  tmp=strtok(NULL,".-:");
  DBG dbgLog(LOG_INFO,"[NET] m:%s ",tmp);
  mytm.tm_min = atoi(tmp);        //0-59

  tmp=strtok(NULL,".-:");
  DBG dbgLog(LOG_INFO,"[NET] s:%s\n",tmp);
  mytm.tm_sec = atoi(tmp);        //0-59
  
  time_t t = mktime(&mytm);

  tv.tv_sec = t;
  tv.tv_usec = 0;

  settimeofday(&tv, NULL);
}


// Returns in lip current IP depending on WiFi mode
//
void Return_Current_IP(IPAddress &lip){
  if(WiFi.getMode()==WIFI_STA){
    lip=WiFi.localIP();
  }else if(WiFi.getMode()==WIFI_AP){
    lip=WiFi.softAPIP();
  }
}


// Turns off WiFi
//
void Disable_WiFi(){
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}


// Starts WiFi in SoftAP mode
//
boolean Start_WiFi_AP(){
  if(!Prefs[PRF_WIFI_MODE].value.uint8) return 1;   // if WiFi disabled

  WiFi.mode(WIFI_AP);
  
  IPAddress local_IP(192,168,10,1);
  IPAddress gateway(192,168,10,1);
  IPAddress subnet(255,255,255,0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  DBG dbgLog(LOG_INFO,"[NET] Creating WiFi Access Point (AP)");
  WiFi.softAP(DEFAULT_AP, DEFAULT_PASS, 8);
  return 0;
}


// Tries to connect to WiFi AP
//
boolean Start_WiFi_CLIENT(){

  if(!Prefs[PRF_WIFI_MODE].value.uint8) return 1;   // if WiFi disabled
  if(strlen(Prefs[PRF_WIFI_SSID].value.str)<1 || strlen(Prefs[PRF_WIFI_PASS].value.str)<1) return 1;  // missing SSI or password

  WiFi.mode(WIFI_STA);
   
  WiFi.begin(Prefs[PRF_WIFI_SSID].value.str, Prefs[PRF_WIFI_PASS].value.str);
  DBG dbgLog(LOG_INFO,"[NET] Connecting to WiFi as Client...\n");
    
  for(byte a=0; !Prefs[PRF_WIFI_RETRY_CNT].value.uint8 || a<Prefs[PRF_WIFI_RETRY_CNT].value.uint8; a++){  // if PRF_WIFI_RETRY_CNT - try indefinitely
    delay(1500);
    if (WiFi.status() == WL_CONNECTED) return 0;
    DBG dbgLog(LOG_INFO,"[NET] Connecting to AP WiFi... %d/%d\n",a+1,Prefs[PRF_WIFI_RETRY_CNT].value.uint8);
  }

  DBG dbgLog(LOG_INFO,"[NET] Connecting to AP WiFi failed!\n");
  Disable_WiFi();
  return 1;
}


// Starts WiFi and all networks services
//
boolean Setup_WiFi(){
struct tm timeinfo;

  Setup_start_date();

  if(Prefs[PRF_WIFI_MODE].value.uint8){ // we have WiFi enabled in config - start it
    int err=0;

    if(Prefs[PRF_WIFI_MODE].value.uint8==1 || Prefs[PRF_WIFI_MODE].value.uint8==2){ // 1 - tries as client if failed, be AP; 2 - just try as client
      err=Start_WiFi_CLIENT();
      if(!err){
        configTime(Prefs[PRF_GMT_OFFSET].value.int16, Prefs[PRF_DAYLIGHT_OFFSET].value.int16, Prefs[PRF_NTPSERVER1].value.str, Prefs[PRF_NTPSERVER2].value.str, Prefs[PRF_NTPSERVER3].value.str); // configure RTC clock with NTP server - or at least try
        SETUP_WebServer(); // Setup function for Webserver from PIDKiln_http.ino
        initSysLog();
        return 0;    // all is ok - connected
      }else if(Prefs[PRF_WIFI_MODE].value.uint8==2) return err;  // not connected and won't try be AP
    }
    // Now we try to be AP - if previous failed or not configured

    err=Start_WiFi_AP();
    
    if(!err){ // WiFi enabled in client mode (connected to AP - perhaps with Internet - try it)
      SETUP_WebServer();
      return 0;
    }return err;
  }else{
    Disable_WiFi();
    return 1; // WiFi disabled
  }
}


// Perform WiFi restart - usually when something does not work right - reinitialize it (perhaps preferences has changed)
//
boolean Restart_WiFi(){
  STOP_WebServer();
  delay(100);
  Disable_WiFi();
  delay(500);
  return Setup_WiFi();
}
