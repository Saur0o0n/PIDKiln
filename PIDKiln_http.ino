/*
** HTTP functions
**
**/


// Other variables
//

// Templare preprocessor for debug screen - debug_board.html
//
String debug_board(const String& var){
 FSInfo fs_info;
 SPIFFS.info(fs_info);

 float fileTotalKB = (float)fs_info.totalBytes / 1024.0;
 float fileUsedKB = (float)fs_info.usedBytes / 1024.0;

 float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
 float realFlashChipSize = (float)ESP.getFlashChipRealSize() / 1024.0 / 1024.0;
 float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
 FlashMode_t ideMode = ESP.getFlashChipMode();

 if(var=="CHIP_ID") return String(ESP.getChipId());
 else if (var=="CORE_VERSION") return String(ESP.getCoreVersion());
 else if (var=="SDK_VERSION") return String(ESP.getSdkVersion());
 else if (var=="BOOT_VERSION") return String(ESP.getBootVersion());
 else if (var=="BOOT_MODE") return String(ESP.getBootMode());
 else if (var=="FLASH_CHIP_ID") return String(ESP.getFlashChipId());
 else if (var=="SFLASH_RAM") return String(Serial.print(flashChipSize));
 else if (var=="AFLASH_RAM") return String(Serial.print(realFlashChipSize));
 else if (var=="FLASH_FREQ") return String(flashFreq);
 else if (var=="FLASH_WRITE_MODE") return String((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
 else if (var=="CPU_FREQ") return String(ESP.getCpuFreqMHz());
 else if (var=="TOTAL_KB") return String(fileTotalKB);
 else if (var=="USED_KB") return String(fileUsedKB);
 else if (var=="BLOCK_SIZE") return String(fs_info.blockSize);
 else if (var=="PAGE_SIZE") return String(fs_info.pageSize);
 else if (var=="MAX_OFILES") return String(fs_info.maxOpenFiles);
 else if (var=="MAX_PLENGTH") return String(fs_info.maxPathLength);
 else return String();
}

// Generates kiln programs index - /programs/index.html
//
void generate_index(){
String tmp;
template_str=String();

  Dir dir = SPIFFS.openDir(PRG_DIRECTORY);
  File index,tmpf;

  // Open index for writting
  if(!(index = SPIFFS.open(PRG_DIRECTORY_X("index.html"), "w"))) return;
  
  // Copy index head
  if(tmpf=SPIFFS.open("/prog_beg.txt", "r")){
    DBG Serial.println("Head of index - copying...");
    tmp=tmpf.readString();
    //DBG Serial.println(tmp);
    index.print(tmp);
    tmpf.close();
  }
  while (dir.next()) {
    // List directory
    tmp=dir.fileName();
    tmp=tmp.substring(sizeof(PRG_DIRECTORY)-1);       // remote path from the name
    if(tmp=="index.html") continue;                   // If this is index.html - skip

    File f = dir.openFile("r");
    if(!f) continue;    // Something went wrong... skip
    template_str += "<tr><td><img src=\"/icons/heat.png\" alt=\"[ICO]\"></td>";
    template_str += "<td><a href=\""+String(f.fullName())+"\" target=\"_blank\">"+tmp+"</a></td>";  
    template_str += "<td>"+String(f.size())+"</td>";
    template_str += "<td>"+f.readStringUntil('\n')+"</td>";
    template_str += "<td><a href=/edit/"+tmp+">edit</a></td>";
    template_str += "<td><a href=/delete/"+tmp+">delete</a></td>";
    template_str += "</tr>\n";
  }
  //DBG Serial.print(template_str);
  index.print(template_str);
  // Copy end of the index template
  if(tmpf=SPIFFS.open("/prog_end.txt", "r")){
    //DBG Serial.println("End of index - copying...");
    tmp=tmpf.readString();
    //DBG Serial.println(tmp);
    index.print(tmp);
    tmpf.close();
  }
  index.flush();
  index.close(); 
}


// Template preprocessor for main view - index.html
//
String processor(const String& var) {
String tmp;

  Serial.println(var);
  if (var == "STATE") {
    template_str = "OFF";
    Serial.print(template_str);
    return template_str;
  }

  return String();
}


// Manages new program upload
// Abort if file too big or contains not allowed characters
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
static File newFile;
static boolean abort=false;
String tmp=PRG_DIRECTORY;

  tmp.concat(filename.c_str());
  
  // Abort file upload if file is too large or there are some not allowed characters
  if(abort){
    abort=false;
    delete_file(newFile);
    request->client()->abort();
    return;
  }

  // Checking how much has been uploaded - if more then max_prog_size - abort
  if(len+index>max_prog_size){
     DBG Serial.println("Uploaded file too large! Aborting");
     request->send(200, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(max_prog_size)+"<br><br><a href=/>Return to main view</a></body></html");
     abort=true;
     return;
  }
    
  if(!index){
    DBG Serial.printf("UploadStart: %s\n", tmp.c_str() );
    if (newFile) newFile.close();
    newFile = SPIFFS.open( tmp.c_str(), "w");
  }
  DBG Serial.println(" Next iteration of file upload...");
  for(size_t i=0; i<len; i++){
    if(!check_valid_chars(data[i])){ // Basic sanitization - check for allowed characters
      request->send(200, "text/html", "<html><body><h1>File contains not allowed character(s)!</h1> You can use all letters, numbers and basic symbols in ASCII code.<br><br><a href=/>Return to main view</a></body></html");
      delete_file(newFile);
      abort=true;
      return;
    }else newFile.write(data[i]);
  }
  if(final){
    newFile.flush();
    newFile.close();
    DBG Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index+len);
    DBG Serial.printf("Checking uploaded program structure\n");
    //validate_program();
    generate_index();
    request->redirect("/programs");
  }
}
