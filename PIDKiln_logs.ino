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
  DBG Serial.printf("[LOG] Trying to create log file:%s\n",str);
  
  if(CSVFile=SPIFFS.open(str, "w")){
    DBG Serial.printf("[LOG] Created new log file %s\n",str);
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
      DBG Serial.printf("[LOG] Failed to create .log file: %s\n",str);
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

  DBG Serial.printf("[LOG] Writing to log file:%s\n",tmp.c_str());
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
    LOGFile.printf("End temperature: %.1fC",kiln_temp);
    if(Energy_Wattage) LOGFile.printf("Used power: %.1f W/h\n",Energy_Wattage);
    if(Program_error){
      LOGFile.printf("Program aborted with error: %d\n",Program_error);
    }
    LOGFile.flush();
    LOGFile.close();
  }
  Generate_LOGS_INDEX();
}


// Load logs directory into memory - to sort it etc for easier processing PSMmem is plenty
//
uint8_t Load_LOGS_Dir(){
uint16_t count=0;
File dir,file;

  dir = SPIFFS.open(LOG_Directory);
  if(!dir) return 1;  // directory open failed
  DBG Serial.println("[LOG] Loading dir: Loading directory...");
  while(dir.openNextFile()) count++;  // not the prettiest - but we count files first to do proper malloc without fragmenting memory
  
  DBG Serial.printf("[LOG] Loading dir:\tcounted %d files\n",count);
  if(Logs_DIR) free(Logs_DIR);
  Logs_DIR=(DIRECTORY*)ps_malloc(sizeof(DIRECTORY)*count);
  Logs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Logs_DIR_size<=count){    // now we do acctual loading into memory
    char* fname;
    char tmp[32];
    uint8_t len2;
    
    strcpy(tmp,file.name());
    len2=strlen(tmp);
    if(len2>31 || len2<2) return 2; // file name with dir too long or just /
    fname=strchr(tmp+1,'/');        // seek for the NEXT directory separator...
    fname++;                        //  ..and skip it
    if(!strcmp(fname,"index.html")) continue;   // skip index file
    
    strcpy(Logs_DIR[Logs_DIR_size].filename,fname);
    Logs_DIR[Logs_DIR_size].filesize=file.size();
    Logs_DIR[Logs_DIR_size].sel=0;
    
    DBG Serial.printf("[LOG] FName: %s\t FSize:%d\tSel:%d\n",Logs_DIR[Logs_DIR_size].filename,Logs_DIR[Logs_DIR_size].filesize,Logs_DIR[Logs_DIR_size].sel);
    
    Logs_DIR_size++;
  }
  dir.close();

  // sorting... slooow but easy
  bool nok=true;
  while(nok){
    nok=false;
    if(Logs_DIR_size>1) // if we have at least 2 progs
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

  //if(Logs_DIR_size) Logs_DIR[0].sel=1; // make first program seleted if we have at least one
  return 0;
}
