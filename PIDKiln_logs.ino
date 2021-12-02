/*
** Logging functionality
**
*/

double EnW_last=0;    // last energy consumption for csv report


// Starts a new log file
//
void Init_log_file(){
char str[33];
struct tm timeinfo, *tmm;

  if(CSVFile) CSVFile.close();
  if(getLocalTime(&timeinfo)) strftime(str, 32, "/logs/%y%m%d_%H%M%S.csv", &timeinfo); //YYMMDD_HHMMSS.log
  else sprintf(str,"/logs/%d.csv",millis());  // if we don't have a clock - use millis - this should NOT happend
  DBG dbgLog(LOG_INFO,"[LOG] Trying to create log file:%s\n",str);
  
  if(CSVFile=SPIFFS.open(str, "w")){
    DBG dbgLog(LOG_INFO,"[LOG] Created new log file %s\n",str);
#ifdef ENERGY_MON_PIN
    CSVFile.print(String("Date,Temperature,Housing,Energy"));
#else
    CSVFile.print(String("Date,Temperature,Housing"));
#endif
  }

  // Try to create additional info-log file if not.. well
  char *tmpstr;
  tmpstr=strstr(str,".csv");
  if(tmpstr){
    strncpy(tmpstr,".log",4);
    if(LOGFile=SPIFFS.open(str, "w")){
      LOGFile.printf("Program name: %s\n",Program_run_name);
      LOGFile.printf("Program desc: %s\n",Program_run_desc);
      if(tmm = localtime(&Program_run_start)){
        strftime(str, 29, "%F %T", tmm);
        LOGFile.printf("Started at: %s\n", str);
      }
      if(tmm = localtime(&Program_run_end)){
        strftime(str, 29, "%F %T", tmm);
        LOGFile.printf("Possible end at: %s\n", str);
      }
      LOGFile.printf("PID values. Kp:%.2f Ki:%.2f Kd:%.2f\n",Prefs[PRF_PID_KP].value.vfloat,Prefs[PRF_PID_KI].value.vfloat,Prefs[PRF_PID_KD].value.vfloat);
      LOGFile.printf("Start temperature: %.1fC\n",kiln_temp);
      LOGFile.printf("CSV filename: %s\n-=-=-= Starting program =-=-=-=-\n",str);
      LOGFile.flush();
//      LOGFile.close();
    }else{
      DBG dbgLog(LOG_ERR,"[LOG] Failed to create .log file: %s\n",str);
    }
  }
  
  EnW_last=0;   // reset last energy consumption with new program
  
  Generate_LOGS_INDEX();
}


// Add single log line with current date and temperature
//
void Add_log_line(){
String tmp;
char str[33];
struct tm timeinfo,*tmm;
                 
  if(!CSVFile) return;
  
  if(getLocalTime(&timeinfo)) strftime(str, 29, "%F %T", &timeinfo);
  else sprintf(str,"%d",millis());

#ifdef ENERGY_MON_PIN
    tmp=String(str)+","+String(kiln_temp,0)+","+String(case_temp,0)+","+String((int)(Energy_Usage-EnW_last));
    EnW_last=Energy_Usage;
#else
    tmp=String(str)+","+String(kiln_temp,0)+","+String(case_temp,0);
#endif

  DBG dbgLog(LOG_INFO,"[LOG] Writing to log file:%s\n",tmp.c_str());
  CSVFile.println();
  CSVFile.print(tmp);
  CSVFile.flush();
}


// Closes cleanly log file
//
void Close_log_file(){
struct tm timeinfo, *tmm;
char str[33];

  if(CSVFile){
    CSVFile.flush();
    CSVFile.close();
  }
  if(LOGFile){
    if(tmm = localtime(&Program_run_end)){
      strftime(str, 29, "%F %T", tmm);
      LOGFile.printf("Program ended at: %s\n", str);
    }
    LOGFile.printf("End temperature: %.1fC\n",kiln_temp);
    if(Energy_Wattage) LOGFile.printf("Used power: %.1f W/h\n",Energy_Wattage);
    if(Program_error){
      LOGFile.printf("Program aborted with error: %d\n",Program_error);
    }
    LOGFile.flush();
    LOGFile.close();
  }
  Clean_LOGS();
  Generate_LOGS_INDEX();
}


// Function leans logs on SPIFFs depending on preferences value "LOG_Files_Limit"
//
void Clean_LOGS(){
char fname[MAX_FILENAME];

  if(Logs_DIR_size<=Prefs[PRF_LOG_LIMIT].value.uint16) return;
  DBG dbgLog(LOG_INFO,"[LOG] Cleaning logs...\n");
  for(uint16_t a=Prefs[PRF_LOG_LIMIT].value.uint16; a<Logs_DIR_size; a++){
    sprintf(fname,"%s/%s",LOG_Directory,Logs_DIR[a].filename);
    DBG dbgLog(LOG_INFO,"[LOG] Deleting file:%s\n",fname);
    SPIFFS.remove(fname);
  }
}


// Load logs directory into memory - to sort it etc for easier processing PSMmem is plenty
//
uint8_t Load_LOGS_Dir(){
uint16_t count=0;
File dir,file;

  dir = SPIFFS.open(LOG_Directory);
  if(!dir) return 1;  // directory open failed
  DBG dbgLog(LOG_INFO,"[LOG] Loading dir: Loading logs directory...\n");
  while(dir.openNextFile()) count++;  // not the prettiest - but we count files first to do proper malloc without fragmenting memory
  
  DBG dbgLog(LOG_INFO,"[LOG] Loading dir:\tcounted %d files\n",count);
  if(Logs_DIR) free(Logs_DIR);
  Logs_DIR=(DIRECTORY*)MALLOC(sizeof(DIRECTORY)*count);
  Logs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Logs_DIR_size<=count){    // now we do acctual loading into memory
    char tmp[32];
    uint8_t len2;
    
    strcpy(tmp,file.name());
    len2=strlen(tmp);
    if(len2>31 || len2<2) return 2; // file name with dir too long or just /
    
    if(!strcmp(tmp,"index.html")) continue;   // skip index file
    
    strcpy(Logs_DIR[Logs_DIR_size].filename,tmp);
    Logs_DIR[Logs_DIR_size].filesize=file.size();
    Logs_DIR[Logs_DIR_size].sel=0;
    
    DBG dbgLog(LOG_DEBUG,"[LOG] FName: %s\t FSize:%d\tSel:%d\n",Logs_DIR[Logs_DIR_size].filename,Logs_DIR[Logs_DIR_size].filesize,Logs_DIR[Logs_DIR_size].sel);
    
    Logs_DIR_size++;
  }
  dir.close();

  // sorting... slooow but easy
  bool nok=true;
  while(nok){
    nok=false;
    if(Logs_DIR_size>1) // if we have at least 2 logs
      for(int a=0; a<Logs_DIR_size-1; a++){
        DIRECTORY tmp;
        if(strcmp(Logs_DIR[a].filename,Logs_DIR[a+1].filename)<0){
          tmp=Logs_DIR[a];
          Logs_DIR[a]=Logs_DIR[a+1];
          Logs_DIR[a+1]=tmp;
          nok=true;
        }
      }    
   }

  //if(Logs_DIR_size) Logs_DIR[0].sel=1; // make first log seleted if we have at least one
  return 0;
}


/*
** Debuging and logging to syslog
*/


void dbgLog(uint16_t pri, const char *fmt, ...) {

  if(!Prefs[PRF_DBG_SERIAL].value.uint8 && !Prefs[PRF_DBG_SYSLOG].value.uint8) return; // don't waste time and resources if logging off
  
  char *message;
  va_list args;
  size_t initialLen;
  size_t len;
  bool result;

  initialLen = strlen(fmt);

  message = new char[initialLen + 1];

  va_start(args, fmt);
  len = vsnprintf(message, initialLen + 1, fmt, args);
  if (len > initialLen) {
    delete[] message;
    message = new char[len + 1];

    vsnprintf(message, len + 1, fmt, args);
  }
  va_end(args);
    
  if(Prefs[PRF_DBG_SERIAL].value.uint8) Serial.print(message);
  if(Prefs[PRF_DBG_SYSLOG].value.uint8) syslog.log(pri,message);

  delete[] message;
}


void initSysLog(){

  DBG dbgLog(LOG_DEBUG,"[LOG] Trying to enable Syslog\n");
  if(Prefs[PRF_DBG_SYSLOG].value.uint8){
    // check wheter we have all syslog params defined, if not - nullyfi prefs
    if(!strlen(Prefs[PRF_SYSLOG_SRV].value.str) || !Prefs[PRF_SYSLOG_PORT].value.uint16){
      Prefs[PRF_DBG_SYSLOG].value.uint8=0;
      DBG dbgLog(LOG_ERR,"[LOG] Syslog enabled but not configured - disabling syslog\n");
      return;
    }
    syslog.server(Prefs[PRF_SYSLOG_SRV].value.str, Prefs[PRF_SYSLOG_PORT].value.uint16);
    syslog.deviceHostname("PIDKiln-ESP32");
    syslog.appName("PIDKiln");
    syslog.defaultPriority(LOG_KERN);
    syslog.log(LOG_INFO, "Begin syslog");
    DBG dbgLog(LOG_DEBUG,"[LOG] Syslog enabled\n");
  }
}


void initSerial(){
  if(Prefs[PRF_DBG_SERIAL].value.uint8) Serial.begin(115200);
}
