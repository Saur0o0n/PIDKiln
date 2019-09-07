/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


/*
** Core/main program functions
**
*/

// Setup timer function
//
void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}


// Load program from disk to memory - validate it
//
byte add_program_line(String& linia){
uint16_t prg[3];
uint8_t a=0,pos=2;
int multi=1;

  // Looking for line dddd:dddd:dddd (temperature:time in minutes:time in minutes) - assume max 1350:9999:9999
  char p_line[15];
  strcpy(p_line,linia.c_str());
  DBG Serial.printf(" Sanitizing line: '%s'\n",p_line);
  a=linia.length(); // going back to front
  prg[2]=0;prg[1]=prg[0]=0;
  while(a--){
    if(p_line[a]=='\0') continue;
    //DBG Serial.printf(" %d(%d)\n",(byte)p_line[a],a);
    if(p_line[a]<48 || p_line[a]>58) return 3;  // if this are not numbers or : - exit  
    if(p_line[a]==58){ // separator :
      multi=1;
      pos--;
    }else{
      //DBG Serial.printf("pos: %d, multi: %d, prg[%d] is %d, current value:%d\n",pos,multi,pos,prg[pos],p_line[a]-48);
      prg[pos]+=multi*(p_line[a]-48);
      multi*=10;
    }
  }
  if(prg[0]>Prefs[PRF_MAX_TEMP].value.uint16) return 4;   // check if we do not exceed max temperature
  Program[Program_size].temp=prg[0];
  Program[Program_size].togo=prg[1];
  Program[Program_size].dwell=prg[2];
  DBG Serial.printf("Program_pos: %d, Temp: %dC Time to: %ds Dwell: %ds\n",Program_size,Program[Program_size].temp,Program[Program_size].togo,Program[Program_size].dwell);
  Program_size++;
  return 0;
}


// Load program in to the array in memory
//
uint8_t Load_program(char *file){
String line;
char file_path[32];
uint8_t err=0;
uint16_t pos=0;
int sel;
File prg;

  if(file){  // if function got an argument - this can happend if you want to validate new program uploaded by http
    sprintf(file_path,"%s/%s",PRG_Directory,file);
    DBG Serial.printf("Got pointer to load:'%s'\n",file);
    Program_name=String(file);
  }else{
    if((sel=Find_selected_program())<0) return Cleanup_program(1);
    sprintf(file_path,"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
    Program_name=String(Programs_DIR[sel].filename);
  }
  DBG Serial.printf("Load program name: '%s'\n",file_path);
  
  if(prg = SPIFFS.open(file_path,"r")){
    Program_desc="";  // erase current program
    Program_size=0;
    while(prg.available()){
      line=prg.readStringUntil('\n');
      line.trim();
      if(!line.length()) continue; // empty line - skip it
      
      DBG Serial.printf("Raw line: '%s'\n",line.c_str());
      if(line.startsWith("#")){ // skip every comment line
        DBG Serial.println("  comment");
        if(!Program_desc.length()){
          line=line.substring(1);  // If it's the first line with comment - copy it without trailing #
          line.trim();
          Program_desc=line;
        }
      }else{
        // Sanitize every line - if it's have a comment - strip ip, then check if this are only numbers and ":" - otherwise fail
        if(pos=line.indexOf("#")){
          line=line.substring(0,pos);
          line.trim(); // trim again after removing comment
        }
        
        if(line.length()>15) return Cleanup_program(2);  // program line too long
        else if(err=add_program_line(line)) return Cleanup_program(err); // line adding failed!!
        
        DBG Serial.printf("San line: '%s'\n",line.c_str());
      }
    }
    DBG Serial.printf("Found description: %s\n",Program_desc.c_str());
    if(!Program_desc.length()) Program_desc="No description";   // if after reading file program still has no description - add it
    
    return 0;
  }return Cleanup_program(1);
}


// Load programs directory into memory - to sort it etc for easier processing
//
uint8_t Load_programs_dir(){
uint16_t count=0;
File dir,file;

  dir = SPIFFS.open(PRG_Directory);
  if(!dir) return 1;  // directory open failed
  DBG Serial.println("Loading directory...");
  while(dir.openNextFile()) count++;  // not the prettiest - but we count files first to do proper malloc without fragmenting memory
  DBG Serial.printf("\tcounted %d files\n",count);
  if(Programs_DIR) free(Programs_DIR);
  Programs_DIR=(DIRECTORY*)malloc(sizeof(DIRECTORY)*count);
  Programs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Programs_DIR_size<=count){    // now we do acctual loading into memory
    char* fname;
    char tmp[32];
    uint8_t len2;
    
    strcpy(tmp,file.name());
    len2=strlen(tmp);
    if(len2>31 || len2<2) return 2; // file name with dir too long or just /
    fname=strchr(tmp+1,'/');        // seek for the NEXT directory separator...
    fname++;                        //  ..and skip it
    if(!strcmp(fname,"index.html")) continue;   // skip index file
    strcpy(Programs_DIR[Programs_DIR_size].filename,fname);
    Programs_DIR[Programs_DIR_size].filesize=file.size();
    Programs_DIR[Programs_DIR_size].sel=0;
    
    DBG Serial.printf("FName: %s\t FSize:%d\tSel:%d\n",Programs_DIR[Programs_DIR_size].filename,Programs_DIR[Programs_DIR_size].filesize,Programs_DIR[Programs_DIR_size].sel);
    
    Programs_DIR_size++;
  }
  dir.close();

  // sorting... slooow but easy
  bool nok=true;
  while(nok){
    nok=false;
    if(Programs_DIR_size>1) // if we have at least 2 progs
      for(int a=0; a<Programs_DIR_size-1; a++){
        DIRECTORY tmp;
        if(strcmp(Programs_DIR[a].filename,Programs_DIR[a+1].filename)>0){
          tmp=Programs_DIR[a];
          Programs_DIR[a]=Programs_DIR[a+1];
          Programs_DIR[a+1]=tmp;
          nok=true;
        }
      }    
   }

  if(Programs_DIR_size) Programs_DIR[0].sel=1; // make first program seleted if we have at least one
  return 0;
}


// Copy selected program to RUN program
//
void Load_program_to_run(){
  
  if(!Program_size) return;
  if(Program_run) free(Program_run);
  if(Program_run_desc) free(Program_run_desc);
  if(Program_run_name) free(Program_run_name);
  Program_run_size=0;
  
  Program_run=(PROGRAM *)malloc(sizeof(PROGRAM)*Program_size);
  for(uint8_t a=0;a<Program_size;a++)
    Program_run[a]=Program[a];
  Program_run_desc=(char *)malloc(Program_desc.length()*sizeof(char)+1);
  strcpy(Program_run_desc,Program_desc.c_str());
  Program_run_name=(char *)malloc(Program_name.length()*sizeof(char)+1);
  strcpy(Program_run_name,Program_name.c_str());
  Program_run_size=Program_size;
  Program_run_state=PR_READY;
}

/*
** Helping functions
**
*/

// Find selected program
//
int Find_selected_program(){
  for(uint16_t a=0; a<Programs_DIR_size; a++) if(Programs_DIR[a].sel>0) return a;
  return -1;  // in case there is NO selected program or there can be no programs at all
}


// Move program selection up/down
//
void rotate_selected_program(int dir){
int a = Find_selected_program();

  DBG Serial.printf("Rotating programs. For a:%d, dir: %d, selected?:%d, dir_size:%d\n",a,dir,Programs_DIR[a].sel,Programs_DIR_size);
  if(dir<0 && a>0){   // if we are DOWN down and we can a>0 - do it, if we can't - do nothing
    Programs_DIR[a].sel=0;  // delete old selection
    Programs_DIR[a-1].sel=1;
  }else if(dir>0 && a<(Programs_DIR_size-1)){   // if we are going UP and there is more programs - do it, if it's the end of program list, do nothing
    Programs_DIR[a].sel=0;  // delete old selection
    Programs_DIR[a+1].sel=1;
  }
}


// Short function to cleanup program def. after failed load
//
byte Cleanup_program(byte err){
  Program_size=0;
  Program_desc="";
  Program_name="";
  for(byte a=0;a<MAX_PRG_LENGTH;a++) Program[a].temp=Program[a].togo=Program[a].dwell=0;
  DBG Serial.printf(" Cleaning up program with error %d\n",err);
  return err;
}


// Erase program from disk
//
boolean Erase_program_file(){
char file[32];

  sprintf(file,"%s/%s",PRG_Directory,Programs_DIR[Find_selected_program()].filename);
  DBG Serial.printf("!!! Erasing file from disk: %s",file);
  return SPIFFS.remove(file);
}












/*
** Main setup and loop function for programs module
*/
void program_setup(){

  // Start interupt timer handler - 1s
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);

  // Start an alarm
  timerAlarmEnable(timer);
}


void program_loop(){

  // check if timer interup occured
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
    uint32_t isrCount = 0, isrTime = 0;
    // Read the interrupt count and time
    portENTER_CRITICAL(&timerMux);
    portEXIT_CRITICAL(&timerMux);

    if(LCD_State==SCR_MAIN_VIEW && LCD_Main==MAIN_VIEW1 && Program_run_size) LCD_display_mainv1();
  }
}
