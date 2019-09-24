/*
** Logging functionality
**
*/

File logfile;

// Starts a new log file
//
void Init_log_file(){
String tmp;
  if(logfile) logfile.close();
  if(logfile=SPIFFS.open("/log/test.csv", "w")){
    DBG Serial.println(" Created new log file");
    tmp=",Temperature";
    logfile.print(tmp);
  }
}


// Add single log line with current date and temperature
//
void Add_log_line(){
String tmp;
char str[30];
time_t current_time;
struct tm timeinfo;
                 
  if(logfile && getLocalTime(&timeinfo)){
    current_time=time(NULL);
    //str=ctime(&current_time);str[strlen(str)-1]='\0';  // Dont know why - probably error, but ctime returns string with new line char and tab - so we cut off the tab
    strftime (str, 29, "%F %T", &timeinfo);
    tmp=String(str)+","+String(kiln_temp,0);
    DBG Serial.printf(" Writing to log file:%s\n",tmp.c_str());
    logfile.println();
    logfile.print(tmp);
    logfile.flush();
  }
}


// Closes cleanly log file
//
void Close_log_file(){
  if(logfile){
    logfile.flush();
    logfile.close();
  }
}
