/*
** Pidkiln program routines - main program functions and all PID magic
**
*/
#include <PID_v1.h>
#include <SPI.h>

// Other variables
//
hw_timer_t *timer = NULL;
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
  //portENTER_CRITICAL_ISR(&timerMux);
  //portEXIT_CRITICAL_ISR(&timerMux);
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
  DBG dbgLog(LOG_DEBUG,"[PRG] Sanitizing line: '%s'\n",p_line);
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
  DBG dbgLog(LOG_DEBUG,"[PRG] Program_pos: %d, Temp: %dC Time to: %ds Dwell: %ds\n",Program_size,Program[Program_size].temp,Program[Program_size].togo,Program[Program_size].dwell);
  Program_size++;
  return 0;
}


// Load program from file to memory - return 0 if success
//
uint8_t Load_program(char *file){
String line;
char file_path[32];
uint8_t err=0;
uint16_t pos=0;
int sel;
File prg;

  if(file){  // if function got an argument - this can happen if you want to validate new program uploaded by http
    sprintf(file_path,"%s/%s",PRG_Directory,file);
    DBG dbgLog(LOG_DEBUG,"[PRG] Got pointer to load:'%s'\n",file);
    Program_name=String(file);
  }else{
    if((sel=Find_selected_program())<0) return Cleanup_program(1);
    sprintf(file_path,"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
    Program_name=String(Programs_DIR[sel].filename);
  }
  DBG dbgLog(LOG_INFO,"[PRG] Load program name: '%s'\n",file_path);
  
  if(prg = SPIFFS.open(file_path,"r")){
    Program_desc="";  // erase current program
    Program_size=0;
    while(prg.available()){
      line=prg.readStringUntil('\n');
      line.trim();
      if(!line.length()) continue; // empty line - skip it
      
      DBG dbgLog(LOG_DEBUG,"[PRG] Raw line: '%s'\n",line.c_str());
      if(line.startsWith("#")){ // skip every comment line
        DBG dbgLog(LOG_DEBUG,"[PRG]  comment");
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
        
        DBG dbgLog(LOG_DEBUG,"[PRG] San line: '%s'\n",line.c_str());
      }
    }
    DBG dbgLog(LOG_DEBUG,"[PRG] Found description: %s\n",Program_desc.c_str());
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
  DBG dbgLog(LOG_INFO,"[PRG] Loading directory...\n");
  while(dir.openNextFile()) count++;  // not the prettiest - but we count files first to do proper malloc without fragmenting memory
  DBG dbgLog(LOG_DEBUG,"[PRG]\tcounted %d files\n",count);
  if(Programs_DIR) free(Programs_DIR);
  Programs_DIR=(DIRECTORY*)MALLOC(sizeof(DIRECTORY)*count);
  Programs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Programs_DIR_size<=count){    // now we do acctual loading into memory
    char tmp[32];
    uint8_t len2;
    
    strcpy(tmp,file.name());
    len2=strlen(tmp);
    if(len2>31 || len2<2) return 2; // file name with dir too long or just /
    DBG dbgLog(LOG_DEBUG,"[PRG] Processing filename: %s\n",tmp);
/* Outdated with ESP32 IC 2.0+
    fname=strchr(tmp+1,'/');        // seek for the NEXT directory separator...
    fname++;                        //  ..and skip it
*/
    if(!strcmp(tmp,"index.html")) continue;   // skip index file
    strcpy(Programs_DIR[Programs_DIR_size].filename,tmp);
    Programs_DIR[Programs_DIR_size].filesize=file.size();
    Programs_DIR[Programs_DIR_size].sel=0;
    
    DBG dbgLog(LOG_DEBUG,"[PRG] FName: %s\t FSize:%d\tSel:%d\n",Programs_DIR[Programs_DIR_size].filename,Programs_DIR[Programs_DIR_size].filesize,Programs_DIR[Programs_DIR_size].sel);
    
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


// Edit program step
//
void Update_program_step(uint8_t sstep, uint16_t stemp, uint16_t stime, uint16_t sdwell){
  if(Program_run_size<=sstep)
    if(Program_run_size==sstep){ // we are out of the program - but this is just NEXT step, we can add
      Program_run_size++;
      Program_run=(PROGRAM *)REALLOC(Program_run,sizeof(PROGRAM)*Program_run_size);
    }else return;   // we are out of the program - we can edit it

  Program_run[sstep].temp=stemp;
  Program_run[sstep].togo=stime;
  Program_run[sstep].dwell=sdwell;
}


// Initizalize program - clear memory
//
void Initialize_program_to_run(){
  if(!Program_run_size) return;
  if(Program_run!=NULL){
    free(Program_run);
    Program_run=NULL;
  }
  if(Program_run_desc!=NULL){
    free(Program_run_desc);
    Program_run_desc=NULL;
  }
  if(Program_run_name!=NULL){
    free(Program_run_name);
    Program_run_name=NULL;
  }
  Program_run_size=0;
  DBG dbgLog(LOG_INFO,"[PRG] Initialized new in-memory program\n");
}


// Copy selected/loaded program to RUN program memory
//
void Load_program_to_run(){
  
  Initialize_program_to_run();
  Program_run=(PROGRAM *)MALLOC(sizeof(PROGRAM)*Program_size);
  for(uint8_t a=0;a<Program_size;a++) Program_run[a]=Program[a];
  
  Program_run_desc=(char *)MALLOC((Program_desc.length()+1)*sizeof(char));
  strcpy(Program_run_desc,Program_desc.c_str());
  Program_run_name=(char *)MALLOC((Program_name.length()+1)*sizeof(char));
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

  DBG dbgLog(LOG_INFO,"[PRG] Rotating programs. For a:%d, dir: %d, selected?:%d, dir_size:%d\n",a,dir,Programs_DIR[a].sel,Programs_DIR_size);
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
  DBG dbgLog(LOG_INFO,"[PRG] Cleaning up program with error %d\n",err);
  return err;
}


// Erase program from disk
//
boolean Erase_program_file(){
char file[32];

  sprintf(file,"%s/%s",PRG_Directory,Programs_DIR[Find_selected_program()].filename);
  DBG dbgLog(LOG_INFO,"[PRG] !!! Erasing file from disk: %s",file);
  return SPIFFS.remove(file);
}


// Cleanly end program
//
void END_Program(){

  DBG dbgLog(LOG_INFO,"[PRG] Ending program cleanly\n");
  Program_run_state=PR_ENDED;
  KilnPID.SetMode(MANUAL);
  Disable_SSR();
  Disable_EMR();
  Program_run_end=time(NULL);
  Close_log_file();
  set_temp=0;
  pid_out=0;
  Program_run_step=-1;
}


// Abort program if something goes wrong - this has no real meaning now - perhaps later...
//
void ABORT_Program(uint8_t error){

  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    Program_error=error;
    DBG dbgLog(LOG_INFO,"[PRG] Aborting program with error: %d\n",Program_error);
    END_Program();
    Program_run_state=PR_ABORTED;
    //Program_run_start=0;
    START_Alarm();
  }
}


// Pause program. We are keeping current temperature and wait until it's resumed
//
void PAUSE_Program(){
  if(Program_run_state==PR_RUNNING) Program_run_state=PR_PAUSED;
}


// Resume pasued program.
//
void RESUME_Program(){
  if(Program_run_state==PR_PAUSED) Program_run_state=PR_RUNNING;
}


// Recalculate ETA based on current step
//
void Program_recalculate_ETA(boolean dwell=false){

  Program_run_end=time(NULL);
  for(uint16_t a=Program_run_step; a<Program_run_size;a++){
    if(!dwell) Program_run_end+=Program_run[a].togo*60;
    else dwell=false;   // If we start counting with dwell=true - next step shoud start as a normal step, so dwell=fasle;
    Program_run_end+=Program_run[a].dwell*60;
  }
}


// This is functional called every second - if program is running. It calculates temperature grow, sets targets etc.
//
void Program_calculate_steps(boolean prg_start=false){
static time_t step_start,next_step_end;
static uint16_t cnt1=1,cnt2=1;
static boolean is_it_dwell=false;

  if(prg_start){  // starting program - do some startup stuff
    Program_run_step=-1; cnt1=0;
    next_step_end=0;         // there was no previous step - so it has ended NOW
    cnt2=999;                // make sure we will parse first step
    is_it_dwell=false;        // last step was dwell
  }

  // Logging stuff - cnt1 - counter for this. If Prefs[PRF_LOG_WINDOW].value.uint16 == 0 - no logging
  cnt1++;
  if(Prefs[PRF_LOG_WINDOW].value.uint16 && cnt1>Prefs[PRF_LOG_WINDOW].value.uint16){
    cnt1=1;
    Add_log_line();
  }
    
  // Program recalc stuff - cnt2 - counter for it
  cnt2++;
  if(cnt2>10){
    cnt2=1;

    if(Program_run_state==PR_PAUSED){   // if we are paused - calculate new ETA end exit
      Program_recalculate_ETA(false);
      next_step_end+=10;                // this is not precise but should be enough
      return;
    }

    if(Prefs[PRF_PID_TEMP_THRESHOLD].value.int16>-1 && set_temp){       // check if we are in threshold window and there is temperature set already - if not, pause
      //DBG Serial.printf("[PRG] Temperature in TEMP_THRESHOLD. Kiln_temp:%.1f Set_temp:%.1f Window:%d\n",kiln_temp,set_temp,Prefs[PRF_PID_TEMP_THRESHOLD].value.int16);
      if(kiln_temp+Prefs[PRF_PID_TEMP_THRESHOLD].value.int16 < set_temp || kiln_temp-Prefs[PRF_PID_TEMP_THRESHOLD].value.int16 > set_temp){   // set_temp must be between kiln_temp +/- temp_threshold 
        DBG dbgLog(LOG_INFO,"[PRG] Temperature in TEMP_THRESHOLD. Kiln_temp:%.1f Set_temp:%.1f Window:%d\n",kiln_temp,set_temp,(int)Prefs[PRF_PID_TEMP_THRESHOLD].value.int16);
        //PAUSE_Program();
        Program_run_state=PR_THRESHOLD;
        Program_recalculate_ETA(false);
        next_step_end+=10; 
        return;
      }else if(Program_run_state==PR_THRESHOLD) Program_run_state=PR_RUNNING;     
    }

    // calculate next step
    if(time(NULL)>next_step_end){
      DBG dbgLog(LOG_DEBUG,"[DBG] Calculating new step!\n");
      if(!is_it_dwell){  // we have finished full step togo+dwell (or this is first step)
        Program_run_step++; // lets icrement step
        
        if(Program_run_step>=Program_run_size){  //we have passed the last step - end program
          END_Program();
          return;
        }
        DBG dbgLog(LOG_DEBUG,"[PRG] Calculating new NORMAL step!\n");
        is_it_dwell=true;   // next step will be dwell
        step_start=time(NULL);
        next_step_end=step_start+Program_run[Program_run_step].togo*60;
        DBG dbgLog(LOG_DEBUG,"[PRG] Next step:%d Start step:%d Togo:%d Run_step:%d/%d Set_temp:%.3f\n",next_step_end,step_start,Program_run[Program_run_step].togo,Program_run_step,Program_run_size,set_temp);
        
        if(Program_run_step>0){ // is this a next step?
          temp_incr=(float)(Program_run[Program_run_step].temp-Program_run[Program_run_step-1].temp)/(Program_run[Program_run_step].togo*60);
          DBG dbgLog(LOG_DEBUG,"[PRG] temp_inc:%f Step_temp:%d Step-1_temp:%d\n",temp_incr,Program_run[Program_run_step].temp,Program_run[Program_run_step-1].temp);
        }else{      // or this is teh first one?
          DBG dbgLog(LOG_DEBUG,"[DBG] First step.\n");
          set_temp=kiln_temp;
          temp_incr=(float)(Program_run[Program_run_step].temp-kiln_temp)/(Program_run[Program_run_step].togo*60);
        }
        Program_recalculate_ETA(false);   // recalculate ETA for normal step
      }else{
        DBG dbgLog(LOG_DEBUG,"[DBG] Calculating new DWELL step!");
        is_it_dwell=false;      // next step will be normal
        temp_incr=0;
        step_start=time(NULL);
        next_step_end=step_start+Program_run[Program_run_step].dwell*60;
        set_temp=Program_run[Program_run_step].temp;
        DBG dbgLog(LOG_DEBUG,"[PRG] Next step:%d Start step:%d Togo:%d Run_step:%d/%d Set_temp:%f\n",next_step_end,step_start,Program_run[Program_run_step].dwell,Program_run_step,Program_run_size,set_temp);
        Program_recalculate_ETA(true);  // recalculate ETA for dwell
      }
    }
    DBG dbgLog(LOG_INFO,"[PRG] Curr temp: %.0f, Set_temp: %.0f, Incr: %.2f Pid_out:%.2f Step: %d\n", kiln_temp, set_temp, temp_incr, pid_out*PID_WINDOW_DIVIDER, Program_run_step);
  }

  if(Program_run_state!=PR_PAUSED && Program_run_state!=PR_THRESHOLD) set_temp+=temp_incr;  // increase desire temperature...
}


// Start running the in memory program
//
void START_Program(){

  if(Program_run_state==PR_NONE || Program_run_state==PR_PAUSED) return; // check if program is ready (we can start also from ended,failed etc,) and not paused
  
  DBG dbgLog(LOG_INFO,"[PRG] Starting new program!");
  Program_run_state=PR_RUNNING;
  Program_start_temp=kiln_temp;
  Energy_Usage=0;
  Program_error=0;
  TempA_errors=TempB_errors=0;  // Reset temperature errors
  
  Enable_EMR();

  KilnPID.SetTunings(Prefs[PRF_PID_KP].value.vfloat,Prefs[PRF_PID_KI].value.vfloat,Prefs[PRF_PID_KD].value.vfloat,Prefs[PRF_PID_POE].value.uint8); // set actual PID parameters
  Program_run_start=time(NULL);
  Program_calculate_steps(true);
  windowStartTime=millis();

  //tell the PID to range between 0 and the full window size/PID_WINDOW_DIVIDER. It's divided by 100 (default value of PID_WINDOW_DIVIDER) to have minimal window size faster above 1/25s - so SSR will be able to switch (zero switch)
  KilnPID.SetOutputLimits(0, Prefs[PRF_PID_WINDOW].value.uint16/PID_WINDOW_DIVIDER);
  KilnPID.SetMode(AUTOMATIC);

  DBG dbgLog(LOG_INFO,"[PRG] Trying to start log - window size:%d\n",Prefs[PRF_LOG_WINDOW].value.uint16);
  if(Prefs[PRF_LOG_WINDOW].value.uint16){ // if we should create log file
    DBG dbgLog(LOG_INFO,"[PRG] Trying to create logs");
    Init_log_file();
    Add_log_line();
  }
}


// Check all possible safety measures - if something is wrong - abort
//
void SAFETY_Check(){
  if(kiln_temp<Prefs[PRF_MIN_TEMP].value.uint8){
    DBG dbgLog(LOG_ERR,"[PRG] Safety check failed - MIN temperature < %d\n",Prefs[PRF_MIN_TEMP].value.uint8);
    ABORT_Program(PR_ERR_TOO_COLD);
  }else if(kiln_temp>Prefs[PRF_MAX_TEMP].value.uint16){
    DBG dbgLog(LOG_ERR,"[PRG] Safety check failed - MAX temperature > %d\n",Prefs[PRF_MAX_TEMP].value.uint16);
    ABORT_Program(PR_ERR_TOO_HOT);  
  }
}


// Function that create task to handle program running
//
void Program_Loop(void * parameter){
static uint16_t cnt1=0;
uint32_t now;

 for(;;){
    
    now = millis();
 
    // Interrupts triggered ones per second
    // 
    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){

      // Update temperature readout
      Update_TemperatureA();

      // Check if there is Alarm ON - if so, lower time and call STOP
      if(ALARM_countdown>0){
        if(ALARM_countdown<=1) STOP_Alarm();
        ALARM_countdown--;
      }

      // Do slow stuff every 10th second - cnt1=[0..9]
      //
      if(cnt1>9) cnt1=0;
      else cnt1++;
#ifdef MAXCS2
      if(cnt1==3){  // just to make it in other time then next if cnt1
        Update_TemperatureB();      // this does not have to be updated so often as kiln temp
      }
#endif
#ifdef ENERGY_MON_PIN
      if(cnt1==4){
        Read_Energy_INPUT();
      }
#endif
      // Do Main view screen refreshing if there is a program and if it's running
      if(LCD_State==SCR_MAIN_VIEW && Program_run_size && LCD_Main==MAIN_VIEW1) LCD_display_mainv1();

      if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED || Program_run_state==PR_THRESHOLD){
        // Do all the program recalc
        Program_calculate_steps();

        if(cnt1==6){
          SAFETY_Check();
        }else if(cnt1==9){
          cnt1=0;
          if(LCD_Main==MAIN_VIEW2 && (Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED)) LCD_display_mainv2();
          DBG dbgLog(LOG_INFO,"[PRG] Pid_out RAW:%.2f Pid_out:%.2f Now-window:%d WindowSize:%d Prg_state:%d\n",pid_out,pid_out*PID_WINDOW_DIVIDER,(now - windowStartTime), Prefs[PRF_PID_WINDOW].value.uint16, (byte)Program_run_state);
        }
       }
    }

    // Do the PID stuff
    if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED || Program_run_state==PR_THRESHOLD){
      KilnPID.Compute();

      if (now - windowStartTime > Prefs[PRF_PID_WINDOW].value.uint16){ //time to shift the Relay Window
        windowStartTime += Prefs[PRF_PID_WINDOW].value.uint16;
      }
      if (pid_out*PID_WINDOW_DIVIDER > now - windowStartTime) Enable_SSR();
      else Disable_SSR();
    }
    yield();
  }
}


/*
** Main setup function for programs module
*/
void Program_Setup(){
 
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

// For testing!!!
//  Load_program("test_up_down.txt");
//  Load_program_to_run();

  xTaskCreatePinnedToCore(
//  xTaskCreate(
              Program_Loop,    /* Task function. */
              "Program_loop",  /* String with name of task. */
              8192,            /* Stack size in bytes. */
              NULL,            /* Parameter passed as input of the task */
              1,               /* Priority of the task. */
              NULL,0);         /* Task handle. */
                    
}
