/*
** HTTP functions
**
*/
#include <soc/efuse_reg.h>
#include <Esp.h>
#include <ESPAsyncWebServer.h>

// Other variables
//
String template_str;  // Stores template pareser output
char *Errors;         // pointer to string if there are some errors during POST


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


/*
** Core/main HTTP functions
**
*/

// Template preprocessor for preferences - preferences.html
//
String Preferences_parser(const String& var){

      if(var=="WiFi_SSID") return String(Prefs[PRF_WIFI_SSID].value.str);
 else if(var=="WiFi_Password") return String(Prefs[PRF_WIFI_PASS].value.str);
 else if(var=="WiFi_Mode0" && Prefs[PRF_WIFI_MODE].value.uint8==0) return "checked";
 else if(var=="WiFi_Mode1" && Prefs[PRF_WIFI_MODE].value.uint8==1) return "checked";
 else if(var=="WiFi_Mode2" && Prefs[PRF_WIFI_MODE].value.uint8==2) return "checked";
 else if(var=="WiFi_Retry_cnt") return String(Prefs[PRF_WIFI_RETRY_CNT].value.uint8);
 
 else if(var=="NTP_Server1") return String(Prefs[PRF_NTPSERVER1].value.str);
 else if(var=="NTP_Server2") return String(Prefs[PRF_NTPSERVER2].value.str);
 else if(var=="NTP_Server3") return String(Prefs[PRF_NTPSERVER3].value.str);
 else if(var=="GMT_Offset_sec") return String(Prefs[PRF_GMT_OFFSET].value.int16);
 else if(var=="Daylight_Offset_sec") return String(Prefs[PRF_DAYLIGHT_OFFSET].value.int16);
 else if(var=="Initial_Date") return String(Prefs[PRF_INIT_DATE].value.str);
 else if(var=="Initial_Time") return String(Prefs[PRF_INIT_TIME].value.str);
 
 else if(var=="MIN_Temperature") return String(Prefs[PRF_MIN_TEMP].value.uint8);
 else if(var=="MAX_Temperature") return String(Prefs[PRF_MAX_TEMP].value.uint16);

 else if(var=="PID_Window") return String(Prefs[PRF_PID_WINDOW].value.uint16);
 else if(var=="PID_Kp") return String(Prefs[PRF_PID_KP].value.vfloat);
 else if(var=="PID_Ki") return String(Prefs[PRF_PID_KI].value.vfloat);
 else if(var=="PID_Kd") return String(Prefs[PRF_PID_KD].value.vfloat);
 else if(var=="PID_POE0" && Prefs[PRF_PID_POE].value.uint8==0) return "checked";
 else if(var=="PID_POE1" && Prefs[PRF_PID_POE].value.uint8==1) return "checked";
 else if(var=="PID_Temp_Threshold") return String(Prefs[PRF_PID_TEMP_THRESHOLD].value.int16);
 
 else if(var=="LOG_Window") return String(Prefs[PRF_LOG_WINDOW].value.uint16);
  
 else if(var=="ERRORS" && Errors){
  String out="<div class=error> There where errors: "+String(Errors)+"</div>";
  DBG Serial.printf("Errors pointer1:%p\n",Errors);
  free(Errors);Errors=NULL;
  return out;
 }
 return String();
}


// Template preprocessor for debug screen - debug_board.html
//
String debug_board(const String& var){

 //return String();

 // Main chip parameters
 //
 if(var=="CHIP_ID"){
   uint64_t chipid;
   char tmp[14];
   chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
   sprintf(tmp,"%04X%08X",(uint16_t)(chipid>>32),(uint32_t)chipid);
   DBG Serial.printf("Chip id: %s\n",tmp);
   return String(tmp);
 }else if (var=="SDK_VERSION") return String(ESP.getSdkVersion());
 else if (var=="CPU_FREQ") return String(ESP.getCpuFreqMHz());
 else if (var=="CHIP_REV") return String(ESP.getChipRevision());
 else if (var=="CHIP_REVF") return String(REG_READ(EFUSE_BLK0_RDATA3_REG) >> 15, BIN);
 // SPI Flash RAM parameters
 //
 else if (var=="SFLASH_RAM"){
   float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
   DBG Serial.printf("flashChipSize: %f\n",flashChipSize);
   return String(flashChipSize);
 }else if (var=="FLASH_FREQ"){
   float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
   DBG Serial.printf("flashFreq: %f\n",flashFreq);
   return String(flashFreq);
 }else if (var=="SKETCH_SIZE"){
   float sketchSize = (float)ESP.getSketchSize() / 1024;
   DBG Serial.printf("sketchSize: %f\n",sketchSize);
   return String(sketchSize);
 }else if (var=="SKETCH_TOTAL"){ // There is an error in ESP framework that shows Total space as freespace - wrong name for the function
   float freeSketchSpace = (float)ESP.getFreeSketchSpace() / 1024;
   DBG Serial.printf("freeSketchSpace: %f\n",freeSketchSpace);
   return String(freeSketchSpace);
 }else if (var=="FLASH_MODE"){
   String mode;
   FlashMode_t fMode = (FlashMode_t)ESP.getFlashChipMode();
   if(fMode==FM_QIO) mode="QIO (0) Quad I/O - Fastest";
   else if(fMode==FM_QOUT) mode="QOUT (1) Quad Output - 15% slower than QIO";
   else if(fMode==FM_DIO) mode="DIO (2) Dual I/O - 45% slower than QIO";
   else if(fMode==FM_DOUT) mode="DOUT (3) Dual Output - 50% slower than QIO";
   else if(fMode==FM_FAST_READ) mode="FAST_READ (4)";
   else if(fMode==FM_SLOW_READ) mode="SLOW_READ (5)";
   else mode="Unknown";
   DBG Serial.printf("flashChipMode: %s\n",mode.c_str());
   DBG Serial.printf("flashChipMode: %d\n",(byte)fMode);
   return String(mode);
 }
 // PSRAM parameters
 //
 else if (var=="TOTAL_PSRAM"){
   float psramSize = (float)ESP.getPsramSize() / 1024;
   DBG Serial.printf("psramSize: %f\n",psramSize);
   return String(psramSize);
 }else if (var=="FREE_PSRAM"){
   float freePsram = (float)ESP.getFreePsram() / 1024;
   DBG Serial.printf("freePsram: %f\n",freePsram);
   return String(freePsram);
 }else if (var=="SMALEST_PSRAM"){
   float minFreePsram = (float)ESP.getMinFreePsram() / 1024;
   DBG Serial.printf("minFreePsram: %f\n",minFreePsram);
   return String(minFreePsram);
 }else if (var=="LARGEST_PSRAM"){
   float maxAllocPsram = (float)ESP.getMaxAllocPsram() / 1024;
   DBG Serial.printf("maxAllocPsram: %f\n",maxAllocPsram);
   return String(maxAllocPsram);
 }
 // RAM parameters
 //
 else if (var=="TOTAL_HEAP"){
   float heapSize = (float)ESP.getHeapSize() / 1024;
   DBG Serial.printf("heapSize: %f\n",heapSize);
   return String(heapSize);
 }else if (var=="FREE_HEAP"){
   float freeHeap = (float)ESP.getFreeHeap() / 1024;
   DBG Serial.printf("freeHeap: %f\n",freeHeap);
   return String(freeHeap);
 }else if (var=="SMALEST_HEAP"){
   float minFreeHeap = (float)ESP.getMinFreeHeap() / 1024;
   DBG Serial.printf("minFreeHeap: %f\n",minFreeHeap);
   return String(minFreeHeap);
 }else if (var=="LARGEST_HEAP"){
   float maxAllocHeap = (float)ESP.getMaxAllocHeap() / 1024;
   DBG Serial.printf("maxAllocHeap: %f\n",maxAllocHeap);
   return String(maxAllocHeap);
 }
 // SPIFFS parameters
 //
 else if (var=="TOTAL_KB"){
   float fileTotalKB = (float)SPIFFS.totalBytes() / 1024.0;
   return String(fileTotalKB);
 }
 else if (var=="USED_KB"){
   float fileUsedKB = (float)SPIFFS.usedBytes() / 1024.0;
   return String(fileUsedKB);
 }
 else return String();
 
 DBG return String();
}


// Generates kiln programs index - /programs/index.html
//
void Generate_INDEX(){
String tmp;
template_str=String();

  File index,tmpf,file;

  if(Load_programs_dir()) return; // can't load directory

  // Open index for writting
  tmp=String(PRG_Directory)+String("/index.html");
  if(!(index = SPIFFS.open(tmp.c_str(), "w"))){
    DBG Serial.println("Failed to open for writing index.html");
    return;
  }

  // Copy index head
  if(tmpf=SPIFFS.open("/prog_beg.txt", "r")){
    DBG Serial.println("Head of index - copying...");
    tmp=tmpf.readString();
    //DBG Serial.println(tmp);
    index.print(tmp);
    tmpf.close();
  }

  for(uint16_t a=0; a<Programs_DIR_size; a++){
    tmp=String(PRG_Directory)+String("/")+String(Programs_DIR[a].filename);
    
    template_str += "<tr><td><img src=\"/icons/heat.png\" alt=\"[ICO]\"></td>";
    template_str += "<td><a href=\""+tmp+"\" target=\"_blank\">"+String(Programs_DIR[a].filename)+"</a></td>";  
    template_str += "<td>"+String(Programs_DIR[a].filesize)+"</td>";
    if(file=SPIFFS.open(tmp.c_str(),"r")){
      template_str += "<td>"+file.readStringUntil('\n')+"</td>";
      file.close();
    }else template_str += "<td> Error opening file to read description </td>";
    template_str += "<td><a href=/load.html?prog_name="+String(Programs_DIR[a].filename)+">load</a></td>";
    template_str += "<td><a href=/delete.html?prog_name="+String(Programs_DIR[a].filename)+">delete</a></td>";
    template_str += "</tr>\n";
  }

  DBG Serial.print(template_str);
  index.print(template_str);
  // Copy end of the index template
  if(tmpf=SPIFFS.open("/prog_end.txt", "r")){
    DBG Serial.println("End of index - copying...");
    tmp=tmpf.readString();
    //DBG Serial.println(tmp);
    index.print(tmp);
    tmpf.close();
  }
  index.flush();
  index.close(); 
}


// Template preprocessor for chart.js
//
String Chart_parser(const String& var) {
String tmp;
time_t current_time;
char *str;

  tmp=String();
  if(Program_run_start) current_time=Program_run_start;
  else current_time=time(NULL);

  if(var == "CHART_DATA" && Program_run_size>0){
    str=ctime(&current_time);str[strlen(str)-1]='\0';  // Dont know why - probably error, but ctime returns string with new line char and tab - so we cut it off
    tmp+="{x:'"+String(str)+"'";
    tmp+=",y:"+String(Program_start_temp)+"},";
    for(uint16_t a=0;a<Program_run_size;a++){
       if(a>0) tmp+=",";
       current_time+=Program_run[a].togo*60;
       str=ctime(&current_time);str[strlen(str)-1]='\0';  // Dont know why - probably error, but ctime returns string with new line char and tab - so we cut the tab
       //DBG Serial.printf("Seconds:%d \t Parsed:'%s'\n",current_time,str);
       tmp+="{x:'"+String(str)+"'";
       tmp+=",y:"+String(Program_run[a].temp)+"},";
       current_time+=Program_run[a].dwell*60;
       str=ctime(&current_time);str[strlen(str)-1]='\0';
       tmp+="{x:'"+String(str)+"'";
       tmp+=",y:"+String(Program_run[a].temp)+"}";
    }
    return tmp;
  }else if(var == "LOG_FILE"){
    if(LOGFile) return LOGFile.name();
    else return String("/log/test.csv");
  }else if(var == "CONFIG"){
    if(LOGFile) return String("config_with");
    else return String("config_without");
  }
  return String();
}


// Template preprocessor for main view - index.html, about and perhaps others
//
String main_parser(const String& var) {
String tmp;

  template_str=String();
  DBG Serial.println(var);
  if (var == "VERSION"){
    template_str+=PVer;
    template_str+=" ";
    template_str+=PDate;
  }
  
  DBG Serial.print(template_str);
  return template_str;
}


// Manages new program upload
// Abort if file too big or contains not allowed characters
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
static File newFile;
static boolean abort=false;
String tmp=String(PRG_Directory);


  tmp.concat("/");
  tmp.concat(filename.c_str());

  // Abort file upload if file is too large or there are some not allowed characters in program
  if(abort){
    abort=false;
    if(newFile) delete_file(newFile);
    request->client()->abort();
    return;
  }


  // Checking how much has been uploaded - if more then MAX_Prog_File_Size - abort
  if(len+index>MAX_Prog_File_Size){
     DBG Serial.println("Uploaded file too large! Aborting");
     request->send(406, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_File_Size)+"<br><br><a href=/>Return to main view</a></body></html");
     abort=true;
     return;
  }
    
  if(!index){
    DBG Serial.printf("UploadStart: %s\n", tmp.c_str() );
    
    // Check if declared file size in header is not too large
    if(request->hasHeader("Content-Length")){
      AsyncWebHeader* h = request->getHeader("Content-Length");
      if(h->value().toInt()>MAX_Prog_File_Size){
        DBG Serial.println("Uploaded file too large! Aborting");
        request->send(406, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_File_Size)+"<br><br><a href=/programs/>Return to programs view</a></body></html");
        abort=true;
        return;
      }
    }

    // Abort if filename is too long (otherwise esp will not write file to SPIFFS silently!)
    if(tmp.length()>MAX_FILENAME){
      DBG Serial.println("Uploaded filename is too large! Aborting");
      request->send(406, "text/html", "<html><body><h1>Filename is too long!</h1> Current limit is "+String(MAX_FILENAME)+"letters for directory and filename, so program name can be only "+String(MAX_PROGNAME)+" <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }

    char tmp_filename[MAX_PROGNAME];
    strcpy(tmp_filename,filename.c_str());
    // Abort if filename contains not allowed characters or trys to overwrite index.html
    if(!valid_filename(tmp_filename) || filename.compareTo("index.html")==0){
      DBG Serial.println("Uploaded filename containg bad characters! Aborting");
      request->send(406, "text/html", "<html><body><h1>Filename is bad!</h1> Filename contains not allowed characters - use letters, numbers and . _ signs <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }
    
    if (newFile) newFile.close();
    newFile = SPIFFS.open( tmp.c_str(), "w");
  }
  DBG Serial.println(" Next iteration of file upload...");
  for(size_t i=0; i<len; i++){
    if(!check_valid_chars(data[i])){ // Basic sanitization - check for allowed characters
      request->send(200, "text/html", "<html><body><h1>File contains not allowed character(s)!</h1> You can use all letters, numbers and basic symbols in ASCII code.<br><br><a href=/>Return to main view</a></body></html");
      delete_file(newFile);
      DBG Serial.printf("Basic program check failed!\n");
      abort=true;
      return;
    }else newFile.write(data[i]);
  }
  if(final){
    newFile.flush();
    DBG Serial.printf("UploadEnd: %s, %d B\n", newFile.name(), newFile.size());
    newFile.close();

    char fname[22];
    strcpy(fname,filename.c_str());
    DBG Serial.printf("Checking uploaded program structure: '%s'\n",fname); 
    uint8_t err=Load_program(fname);
    
    if(err){  // program did not validate correctly
      request->send(200, "text/html", "<html><body><h1>Program stucture is incorrect!</h1> Error code "+String(err)+".<br><br><a href=/programs/>Return to programs</a></body></html");
      delete_file(newFile=SPIFFS.open( tmp.c_str(), "r"));  // we need to open file again - to close it with already existing function
      DBG Serial.printf("Detailed program check failed!\n");
      abort=true; // this will never happend...
      request->redirect("/programs");
    }else{ // Everything went fine - commit file
      Generate_INDEX();
      request->redirect("/programs");
    }
  }
}


// Handle delete - second run, post - actual deletion
//
void delete_handle_post(AsyncWebServerRequest *request){

  //Check if POST (but not File) parameter exists
  if(request->hasParam("prog_name", true) && request->hasParam("yes", true)){
    AsyncWebParameter* p = request->getParam("yes", true);
    if(p->value().compareTo(String("Yes!"))==0){ // yes user want's to delete program
      AsyncWebParameter* p = request->getParam("prog_name", true);
      char path[32];
      sprintf(path,"%s/%.*s",PRG_Directory,MAX_PROGNAME,p->value().c_str());
      DBG Serial.printf(" Removing program: %s with fpath:%s\n",p->value().c_str(),path);
      if(SPIFFS.exists(path)){
        SPIFFS.remove(path);
        Generate_INDEX();
      }
    }
  }
  request->redirect("/programs");
}


// Handle delete - first run, get - are you sure question
//
void delete_handle_get(AsyncWebServerRequest *request){
File tmpf;
String tmps;

  DBG Serial.printf(" Request type: %d\n",request->method());
  DBG Serial.printf(" Request url: %s\n",request->url().c_str());

  if(!request->hasParam("prog_name") || !(tmpf=SPIFFS.open("/delete.html", "r")) ){  // if no program to delete - skip
    DBG Serial.println(" Failed to find GET or open delete.html");
    request->redirect("/programs");
    return;
  }
 
  AsyncWebParameter* p = request->getParam("prog_name");
  
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server","ESP Async Web Server");

  DBG Serial.printf(" Opened file is %s, program name is:%s\n",tmpf.name(),p->value().c_str());
  
  while(tmpf.available()){
    tmps=tmpf.readStringUntil('\n');
    tmps.replace("~PROGRAM_NAME~",p->value());
    //DBG Serial.printf("-:%s:\n",tmps.c_str());
    response->println(tmps.c_str());
  }

  tmpf.close();
  request->send(response);
}


// Handle prefs upload
//
void handlePrefs(AsyncWebServerRequest *request){
boolean save=false;
  
  int params = request->params();
  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()){
      DBG Serial.printf("[HTTP] Prefs parser POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if(p->name().equalsIgnoreCase("save")){
        save=true;
        continue;
      }else if(p->name().equalsIgnoreCase("update")){
        continue;
      }else if(!Change_prefs_value(p->name(),p->value())){
        DBG Serial.printf("[HTTP]!!! We have post error for %s with '%s'\n",p->name().c_str(),p->value().c_str());
        // we have some errors add new field to error list
        if(Errors!=NULL){
          DBG Serial.printf("[HTTP] Realloc call of size %d\n",(strlen(Errors)+p->name().length()+3)*sizeof(char));
          Errors=(char *)ps_realloc(Errors,(strlen(Errors)+p->name().length()+3)*sizeof(char));
          strcat(Errors," ");
          strcat(Errors,p->name().c_str());
          DBG Serial.printf("[HTTP] Errors now:%s\n",Errors);
        }else{
          DBG Serial.printf("[HTTP] Malloc call of size %d\n",(p->name().length()+3)*sizeof(char));
          Errors=strdup(p->name().c_str());
          DBG Serial.printf("[HTTP] Errors now:%s\n",Errors);
        }
      }
    }
  }
  Prefs_updated_hook();
  if(save){
    Save_prefs();
  }
  request->send(SPIFFS, "/preferences.html", String(), false, Preferences_parser);
}


// Handler for index.html with POST - program control
//
void handleIndexPost(AsyncWebServerRequest *request){
int params = request->params();

  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()){
      DBG Serial.printf("[HTTP] Index post parser: POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if(p->name().equalsIgnoreCase("prog_start")){ // start program
        if(Program_run_state==PR_PAUSED) RESUME_Program();
        else START_Program();
      }else if(p->name().equalsIgnoreCase("prog_pause") && Program_run_state==PR_RUNNING){
        PAUSE_Program();
      }else if(p->name().equalsIgnoreCase("prog_end") && (Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED)){
        END_Program();
      }else if(p->name().equalsIgnoreCase("prog_abort")){
        ABORT_Program();
      }
    }
  }
  request->send(SPIFFS, "/index.html", String(), false, main_parser);
}


// Handle json vars for js
//
String handleVars(const String& var){
char str[30];
struct tm *timeinfo,tmm;

  if(var=="KILN_TEMP") return String(kiln_temp);
  else if(var=="SET_TEMP") return String(set_temp);
  else if(var=="ENV_TEMP") return String(int_temp);
  else if(var=="CASE_TEMP") return String(case_temp);
  else if(var=="HEAT_TIME") return String((pid_out/Prefs[PRF_PID_WINDOW].value.uint16)*100);
  else if(var=="TEMP_CHANGE") return String(temp_incr);
  else if(var=="STEP"){
    if(Program_run_state==PR_RUNNING){
      if(temp_incr==0) strcpy(str,"Dwell");
      else strcpy(str,"Running");
    }else strcpy(str,Prog_Run_Names[Program_run_state]);
    return String(Program_run_step+1)+" of "+String(Program_run_size)+" - "+String(str);
  }
  else if(var=="CURR_TIME"){
    struct tm tmm;
    if(getLocalTime(&tmm)){
      strftime (str, 29, "%F %T", &tmm);
      return String(str);
    }
  }else if(var=="PROG_START" && Program_run_start){
    struct tm *tmm;
    tmm = localtime(&Program_run_start);
    strftime (str, 29, "%F %T", tmm);
    return String(str);
  }else if(var=="PROG_END" && Program_run_end){
    struct tm *tmm;
    tmm = localtime(&Program_run_end);
    strftime (str, 29, "%F %T", tmm);
    return String(str);
  }else if(var=="LOG_FILE"){
    if(LOGFile) return LOGFile.name();
    else return String("/log/test.csv");
  }if(var=="PROGRAM_STATUS") return String(Program_run_state);
  else return String(" "); 
}

/* 
** Setup Webserver screen 
**
*/
void setup_webserver(void) {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  });
  
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false, main_parser);
  });
  
  server.on("/index.html", HTTP_POST, handleIndexPost);
  
  server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/about.html", String(), false, main_parser);
  });
  
  server.on("/js/chart.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/chart.js", String(), false, Chart_parser);
  });

  server.on("/PIDKiln_vars.json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/PIDKiln_vars.json", "application/json", false, handleVars);
  });

  // Set default file for programs to index.html - because webserver was programmed by some Windows @%$$# :/
  server.serveStatic("/programs/", SPIFFS, "/programs/").setDefaultFile("index.html");

  // Upload new programs
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, handleUpload);

  server.on("/debug_board.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/debug_board.html", String(), false, debug_board);
  });

  server.on("/preferences.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/preferences.html", String(), false, Preferences_parser);
  });

  server.on("/preferences.html", HTTP_POST, handlePrefs);
  
  server.on("/delete.html", HTTP_GET, delete_handle_get);
  
  server.on("/delete.html", HTTP_POST, delete_handle_post);
  
  // Serve some static data
  server.serveStatic("/icons/", SPIFFS, "/icons/");
  server.serveStatic("/js/", SPIFFS, "/js/");
  server.serveStatic("/css/", SPIFFS, "/css/");
  server.serveStatic("/log/", SPIFFS, "/log/");
  server.serveStatic(PREFS_FILE, SPIFFS, PREFS_FILE);
  server.serveStatic("/favicon.ico", SPIFFS, "/icons/heat.png");

  // Start server
  server.begin();
}
