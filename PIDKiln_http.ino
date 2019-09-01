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

/*
** Core/main HTTP functions
**
*/

// Template preprocessor for preferences - preferences.html
//
String preferences(const String& var){

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
void generate_index(){
String tmp;
template_str=String();

  File index,tmpf,file;

  if(load_programs_dir()) return; // can't load directory

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
    template_str += "<td><a href=/edit/"+String(Programs_DIR[a].filename)+">edit</a></td>";
    template_str += "<td><a href=/delete/"+String(Programs_DIR[a].filename)+">delete</a></td>";
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


// Template preprocessor for main view - index.html, and some others
//
String parser(const String& var) {
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

  // Abort file upload if file is too large or there are some not allowed characters
  if(abort){
    abort=false;
    delete_file(newFile);
    request->client()->abort();
    return;
  }

  // Checking how much has been uploaded - if more then MAX_Prog_Size - abort
  if(len+index>MAX_Prog_Size){
     DBG Serial.println("Uploaded file too large! Aborting");
     request->send(200, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_Size)+"<br><br><a href=/>Return to main view</a></body></html");
     abort=true;
     return;
  }
    
  if(!index){
    DBG Serial.printf("UploadStart: %s\n", tmp.c_str() );
    // Abort if filename is too long (otherwise esp will not write file to SPIFFS silently!)
    if(tmp.length()>MAX_FILENAME){
      DBG Serial.println("Uploaded filename is too large! Aborting");
      request->send(200, "text/html", "<html><body><h1>Filename is too long!</h1> Current limit is "+String(MAX_FILENAME)+"letters for directory and filename, so program name can be only "+String(MAX_PROGNAME)+" <br><br><a href=/>Return to main view</a></body></html");
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
    uint8_t err=load_program(fname);
    
    if(err){  // program did not validate correctly
      request->send(200, "text/html", "<html><body><h1>Program stucture is incorrect!</h1> Error code "+String(err)+".<br><br><a href=/programs/>Return to programs</a></body></html");
      delete_file(newFile=SPIFFS.open( tmp.c_str(), "r"));  // we need to open file again - to close it with already existing function
      DBG Serial.printf("Detailed program check failed!\n");
      abort=true; // this will never happend...
      request->redirect("/programs");
    }else{ // Everything went fine - commit file
      generate_index();
      request->redirect("/programs");
    }
  }
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
    request->send(SPIFFS, "/index.html", String(), false, parser);
  });

  server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/about.html", String(), false, parser);
  });
  
  // Serve some static data
  server.serveStatic("/icons/", SPIFFS, "/icons/");
  server.serveStatic("/js/", SPIFFS, "/js/");
  server.serveStatic("/css/", SPIFFS, "/css/");
  server.serveStatic("/favicon.ico", SPIFFS, "/icons/heat.png");
 
  // Set default file for programs to index.html - because webserver was programmed by... :/
  server.serveStatic("/programs/", SPIFFS, "/programs/").setDefaultFile("index.html");

  // Upload new programs
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, handleUpload);

  server.on("/debug_board.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/debug_board.html", String(), false, debug_board);
  });

  server.on("/preferences.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/preferences.html", String(), false, preferences);
  });
  
  // Start server
  server.begin();
}
