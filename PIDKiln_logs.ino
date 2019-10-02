/*
** Logging functionality
**
*/



// Starts a new log file
//
void Init_log_file(){
char str[33];
struct tm timeinfo;

  if(LOGFile) LOGFile.close();
  if(getLocalTime(&timeinfo)) strftime(str, 32, "/log/%C%m%d_%H%M%S.csv", &timeinfo); //YYMMDD_HHMMSS.log
  else sprintf(str,"/log/%d.csv",millis());  // if we don't have a clock - use millis - this should NOT happend
  DBG Serial.printf("[LOG] Trying to create log file:%s\n",str);
  
  if(LOGFile=SPIFFS.open(str, "w")){
    DBG Serial.printf("[LOG] Created new log file %s\n",str);
    LOGFile.print(String("Date,Temperature,Housing"));
  }

  // Try to create additional info-log file if not.. well
  char *tmpstr;
  tmpstr=strstr(str,".csv");
  if(tmpstr){
    strncpy(tmpstr,".log",4);
    if(File tmpfile=SPIFFS.open(str, "w")){
      tmpfile.printf("Program name:%s\n",Program_run_name);
      tmpfile.printf("Program desc:%s\n",Program_run_desc);
      tmpfile.printf("Started at:%d\n",Program_run_start);
      tmpfile.printf("Possible end at:%d\n",Program_run_end);
      tmpfile.printf("CSV filename:%s\n",str);
      tmpfile.flush();
      tmpfile.close();
    }
  }
}


// Add single log line with current date and temperature
//
void Add_log_line(){
String tmp;
char str[30];
struct tm timeinfo;
                 
  if(LOGFile){
    if(getLocalTime(&timeinfo)) strftime(str, 29, "%F %T", &timeinfo);
    else sprintf(str,"%d",millis());
    tmp=String(str)+","+String(kiln_temp,0)+","+String(case_temp,0);
    DBG Serial.printf("[LOG] Writing to log file:%s\n",tmp.c_str());
    LOGFile.println();
    LOGFile.print(tmp);
    LOGFile.flush();
  }
}


// Closes cleanly log file
//
void Close_log_file(){
  if(LOGFile){
    LOGFile.flush();
    LOGFile.close();
  }
}
