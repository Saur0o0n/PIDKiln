/*
** PIDKiln v1.3 - high temperature kiln PID controller for ESP32
**
** Copyright (C) 2019-2022 - Adrian Siemieniak
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************
**
** Some coding convention - used functions are in separate files, depending on what they do. So LCD stuff to LCD, HTTP/WWW to HTTP, rotary to INPUT etc. 
** All variables beeing global are written with capital leter on start of each word (or at least they should be). All definitions are all capital letters.
**
*/


#include "main.h"

/* 
** Static, editable parameters. Some of them, can be replaces with PIDKiln preferences.
** Please set them up before uploading.
*/
#define TEMPLATE_PLACEHOLDER '~' // THIS DOESN'T WORK NOW FROM HERE - replace it in library! Arduino/libraries/ESPAsyncWebServer/src/WebResponseImpl.h

// If you have Wrover with PSRAM
//#define MALLOC ps_malloc
//#define REALLOC ps_realloc

// if you have Wroom without it
#define MALLOC malloc
#define REALLOC realloc

#define DEBUG true
//#define DEBUG false

/*
** LCD Display components
**
*/

// You can switch hardware of software SPI interface to LCD. HW can be up to x10 faster - but requires special pins (and has some errors for me on 5V).
//U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ LCD_CLOCK, /* data=*/ LCD_DATA, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);
//U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Initialize SPI and MAX31855
SPIClass *ESP32_SPI = new SPIClass(HSPI);
MAX31855 ThermocoupleA(MAXCS1);
// Короче надо подключать к VSPI, а релешку переназначить на что-то другое
//Adafruit_MAX31856 maxthermo = Adafruit_MAX31856(MAXCS1);

// If we have defines power meter pins
#ifdef ENERGY_MON_PIN
#include <EmonLib.h>
#define ENERGY_MON_AMPS 30        // how many amps produces 1V on your meter (usualy with voltage output meters it's their max value).
#define EMERGY_MON_VOLTAGE 230    // what is your mains voltage
#define ENERGY_IGNORE_VALUE 0.4   // if measured current is below this - ignore it (it's just noise)
EnergyMonitor emon1;
#endif
uint16_t Energy_Wattage=0;        // keeping present power consumtion in Watts
double Energy_Usage=0;            // total energy used (Watt/time)

// If you have second thermoucouple
#ifdef MAXCS2
MAX31855 ThermocoupleB(MAXCS2);
#endif

boolean SSR_On; // just to narrow down state changes.. I don't know if this is needed/faster

/*
** HTTP functions
**
*/


// Other variables
//
String template_str;  // Stores template pareser output
char *Errors;         // pointer to string if there are some errors during POST

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//

// Vars for interrupt function to keep track on encoder
volatile int lastEncoded = 0;
volatile int encoderValue = 0;
volatile unsigned long encoderButton = 0;

// Global value of the encoder position
int lastencoderValueT = 0;
byte menu_pos=0,screen_pos=0;

// Tell compiler it's interrupt function - otherwise it won't work on ESP
void ICACHE_RAM_ATTR handleInterrupt ();














// Close cleanly file and delete file from SPIFFS
//
boolean delete_file(File &newFile){
char filename[32];
 if(newFile){
    strcpy(filename,newFile.name());
    DBG dbgLog(LOG_DEBUG,"[MAIN] Deleting uploaded file: \"%s\"\n",filename);
    newFile.flush();
    newFile.close();
    if(SPIFFS.remove(filename)){
      DBG dbgLog(LOG_DEBUG,"[MAIN] Deleted!");
    }
    Generate_INDEX(); // Just in case user wanted to overwrite existing file
    return true;
 }else return false;
}


// Function check is uploaded file has only ASCII characters - this to be modified in future, perhaps to even narrowed down.
// Currently excluded are non printable characters, except new line and [] brackets. [] are excluded mostly for testing purpose.
// This way it should be faster then traversing char array
boolean check_valid_chars(byte a){
  if(a==0 || a==9 || a==95) return true; // end of file, tab, _
  if(a==10 || a==13) return true; // new line - Line Feed, Carriage Return
  if(a>=32 && a<=90) return true; // All basic characters, capital letters and numbers
  if(a>=97 && a<=122) return true; // small letters
  DBG Serial.print("[MAIN]  Faulty character ->");
  DBG Serial.print(a,DEC);
  DBG Serial.print(" ");
  DBG Serial.write(a);
  DBG Serial.println();
  return false;
}


// Check filename if it has only letters, numbers and . _ characters - other are not allowed for easy parsing and transfering
//
boolean valid_filename(char *file){
char c;
  while (c = *file++){
    if(strchr(allowed_chars_in_filename,c)) continue; // if every letter is on allowed list - we are good to go
    else return false;
  }
  return true;
}


// Main setup that invokes other subsetups to initialize other modules
//
void setup() {
  char msg[MAX_CHARS_PL];

// This should disable watchdog killing asynctcp and others - one of this should work :)
// This is not recommended, but if Webserver/AsyncTCP will hang (that has happen to me) - this will at least do not reset the device (and potentially ruin program).
// ESP32 will continue to work properly even in AsynTCP will hang - there will be no HTTP connection. If you do not like this - comment out next 6 lines.
  esp_task_wdt_init(1,false);
  esp_task_wdt_init(2,false);
  rtc_wdt_protect_off();
  rtc_wdt_disable();
  disableCore0WDT();
  disableCore1WDT();

  // Initialize prefs array with default values
  Setup_prefs();

  // Serial port for debugging purposes
  initSerial();

  // Initialize SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    DBG dbgLog(LOG_DEBUG,"[MAIN] An Error has occurred while mounting SPIFFS\n");
    return;
  }

  // Load all preferences from disk
  Load_prefs();
  
  // Setup function for LCD display from PIDKiln_LCD.ino
  Setup_LCD();

  // Setup input devices
  Setup_Input();
  
  DBG dbgLog(LOG_DEBUG,"WiFi mode: %d, Retry count: %d, is wifi enabled: %d\n",Prefs[PRF_WIFI_MODE].value.uint8,Prefs[PRF_WIFI_RETRY_CNT].value.uint8,Prefs[PRF_WIFI_SSID].type);
  
  // Connect to WiFi if enabled
  if(Prefs[PRF_WIFI_MODE].value.uint8){ // If we want to have WiFi
    strcpy(msg,"connecting WiFi..");
    load_msg(msg);
    if(Setup_WiFi()){    // !!! Wifi connection FAILED
      DBG dbgLog(LOG_ERR,"[MAIN] WiFi connection failed\n");
      strcpy(msg," WiFi con. failed");
      load_msg(msg);
    }else{
      IPAddress lips;
     
      Return_Current_IP(lips); 
      DBG Serial.println(lips); // Print ESP32 Local IP Address
      
      sprintf(msg," IP: %s",lips.toString().c_str());
      load_msg(msg);
    }
  }else{
    // If we don't have Internet - assume there is no time set
    Setup_start_date(); // in PIDKiln_net
    Disable_WiFi();
    strcpy(msg,"   -- Started! --");
    load_msg(msg);
  }

  // (re)generate programs index file /programs/index.html
  Generate_INDEX();

  // Loads logs index
  Load_LOGS_Dir();
  
  // Clean logs (if neede) on start - this will also call logs index generator
  Clean_LOGS();

  // Generate log index after cleanup
  Generate_LOGS_INDEX();

  // Setup program module
  Program_Setup();

  // Setup all sensors/relays
  Setup_Addons();
}


// Just a tiny loop - to be deleted ;)
//
void loop() {
  vTaskDelete(NULL);
}

/*
** LCD Display components
**
*/


/*
** Helping functions
**
*/




// Cut string for LCD width, return 1 if there's something left
// (input string, rest to output, screen width modificator, screen width, default = SCREEN_W)
boolean return_LCD_string(char* msg,char* rest, int mod, uint16_t screen_w){
uint16_t chh,lnw;
char out[MAX_CHARS_PL]; 

  chh=u8g2.getMaxCharHeight();
  //DBG Serial.printf("[LCD] Line cut: Got:%s\n",msg);
  lnw=floor((SCREEN_W+mod)/u8g2.getMaxCharWidth())-1; // max chars in line
  if(strlen(msg)<=lnw){
    rest[0]='\0';
    //DBG Serial.printf("[LCD] Line cut: line shorter then %d - skipping\n",lnw);
    return false;
  }
  strncpy(rest,msg+lnw+1,strlen(msg)-lnw);
  rest[strlen(msg)-lnw]='\0';
  msg[lnw+1]='\0';
  //DBG Serial.printf("[LCD] Line cut. Returning msg:'%s' and rest:'%s'\n",msg,rest);
  return true;
}


// Write short messages during starting
//
void load_msg(char msg[MAX_CHARS_PL]){
  u8g2.setDrawColor(0);
  u8g2.drawBox(3, 31, 121, 22);
  u8g2.setDrawColor(1);
  u8g2.drawStr(10,45,msg);
  u8g2.sendBuffer();
}


// Draws dotted vertical line
//
void DrawVline(uint16_t x,uint16_t y,uint16_t h){

  //DBG Serial.printf("[LCD] -> Draw V dots: x:%d\t y:%d\t h:%d\n",x,y,h);
  h+=y;
  for(uint16_t yy=y;yy<h;yy+=3) u8g2.drawPixel(x,yy);
}


// Draw string with one char inversed
// 
void Draw_Marked_string(char *str,uint8_t pos){
uint8_t a;
  u8g2.setFontMode(0);
  for(a=0;a<strlen(str);a++){
    if(a==pos){
      u8g2.setDrawColor(0);
      u8g2.print(str[a]);
      u8g2.setDrawColor(1);
    }else u8g2.print(str[a]);
  }
  u8g2.setFontMode(1);
}


// Draws one element of horizontal menu
// msg = text to draw, y - box start point (lower,left corner - like with text in u8g2), cnt - fraction (split to 2, 3 etc), el - which element of cnt is this, sel - should element be in-versed
void DrawMenuEl(char *msg, uint16_t y, uint8_t cnt, uint8_t el, boolean sel){
uint16_t chh,x_w=0,width=0,x=0,x_txt=0;
char rest[MAX_CHARS_PL];

  chh=u8g2.getMaxCharHeight()+2;
  if(cnt>1) x_w=floor((float)SCREEN_W/cnt);
  else x_w=SCREEN_W;
  el--;   // because we want to count 0.. not 1..
  x+=x_w*el;   // we start at 0 and el*button width
  
  if(cnt==el+1) x_w=SCREEN_W-x; // just make sure to draw last square to the end
  else x_w++; // some shitty voodoo
  
  return_LCD_string(msg,rest,2,x_w);  // cut text size if too long - this is for safety, should not happend
  width=u8g2.getStrWidth(msg);
  x_txt=x+(x_w-2-width)/2;            // find out how to put a text in the middle of the button

  u8g2.drawFrame(x,y-chh,x_w,chh);  // draw frame around text w+1 because start point is also counted to witdh
  DBG dbgLog(LOG_DEBUG,"[LCD] Width:%d cnt:%d el:%d x:%d x_txt:%d x_w:%d\n",SCREEN_W,cnt,el,x,x_txt,x_w);
  
  if(sel){
      u8g2.drawBox(x, y-chh, x_w, chh);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x_txt,y-1,msg);
      u8g2.setDrawColor(1);
  }else u8g2.drawStr(x_txt,y-1,msg);          // draw text - we should check it's size also..
}

/*
** Core/main LCD functions
**
*/

// Third main view - operations (start,pause,stop)
//
void LCD_display_mainv3(int dir/*=0*/, byte ctrl/*=0*/){
  char msg[MAX_CHARS_PL];
  uint16_t chh,chw,x=2,y=2;
  static int what=0;

  if(!Program_run_size) return; //  don't go in if no program loaded
  LCD_State=SCR_MAIN_VIEW;
  LCD_Main=MAIN_VIEW3;  // just in case...

  if(dir==0 && ctrl==0) what=0; // initialize what
  
  what+=dir;    // rotate 1/-1
  
  if(what>4){  // we can rotate out of this screen
    LCD_Main=MAIN_VIEW1;
    LCD_display_main_view();
    return;
  }else if(what<0){
    LCD_Main=MAIN_VIEW2;
    LCD_display_main_view();
    return;
  }
  if(ctrl==2){
    switch(what){
      case 0:
        START_Program();
        break;
      case 1:
        if(Program_run_state==PR_PAUSED) RESUME_Program();
        else PAUSE_Program();
        break;
      case 3:
        END_Program();
        break;
      case 4:
        ABORT_Program(PR_ERR_USER_ABORT);
        break;
      default:
        break;
    }
    LCD_Main=MAIN_VIEW1;
    LCD_display_main_view();
    return;
  }
  
  u8g2.clearBuffer();
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()+1;
  chw=u8g2.getMaxCharWidth();

  y=SCREEN_H-chh;
  sprintf(msg,"Start");
  DrawMenuEl(msg,y,3,1,(what==0));
  if(Program_run_state==PR_PAUSED) sprintf(msg,"Resume");
  else sprintf(msg,"Pause");
  DrawMenuEl(msg,y,3,2,(what==1));
  sprintf(msg,"Return");
  DrawMenuEl(msg,y,3,3,(what==2));

  y=SCREEN_H;
  sprintf(msg,"Stop");
  DrawMenuEl(msg,y,2,1,(what==3));
  sprintf(msg,"Abort");
  DrawMenuEl(msg,y,2,2,(what==4));
  
  u8g2.sendBuffer();
}


// Second main view - program graph
//
void LCD_display_mainv2(){
uint16_t ttime=0,mxtemp=0,mxx=0,mxy=0,x,y,oldx,oldy,scx,scy,startx,starty;
char msg[MAX_CHARS_PL];

  if(!Program_run_size) return; //  don't go in if no program loaded
  LCD_State=SCR_MAIN_VIEW;
  LCD_Main=MAIN_VIEW2;  // just in case...

  u8g2.clearBuffer();
  u8g2.setFont(FONT4);
  u8g2.setFontPosBottom();
  for(uint8_t a=0; a<Program_run_size; a++){
    ttime+=Program_run[a].togo+Program_run[a].dwell;
    if(Program_run[a].temp>mxtemp) mxtemp=Program_run[a].temp;
  }

  mxx=SCREEN_W-2; // max X axis - time
  mxy=SCREEN_H-5; // max Y axis - temperature
  if(ttime) scx=(int)((mxx*100)/ttime);   // 1 minute is scx pixels * 100 on graph 
  if(mxtemp) scy=(int)((mxy*100)/mxtemp); // 1 celsius is scy pixel * 100

  DBG dbgLog(LOG_DEBUG,"[LCD] Graph. mxx:%d mxy:%d ttime:%d mxtemp:%d scx:%d scy:%d\n",mxx,mxy,ttime,mxtemp,scx,scy);

  // Draw axies
  u8g2.drawHLine(1,SCREEN_H-1,mxx);
  u8g2.drawVLine(1,SCREEN_H-mxy,mxy);
  u8g2.drawHLine(0,SCREEN_H-mxy,3);
  sprintf(msg,"%dC",mxtemp);
  u8g2.drawStr(4,9,msg);
  
  startx=1;
  starty=SCREEN_H-1;  // 0.0 on screen is in left, upper corner - reverse it
  
  oldx=startx;
  oldy=SCREEN_H-(Program_start_temp*scy)/100;  // we start from current temp
  
  for(uint8_t a=0; a<Program_run_size; a++){
    x=startx;y=starty;      // in case of any fuckup - jest go to start point
    y=starty-(int)(Program_run[a].temp*scy)/100;
    x=(int)(Program_run[a].togo*scx)/100+oldx;
    u8g2.drawLine(oldx,oldy,x,y);
    // DBG Serial.printf("[LCD].Drawing line: x0:%d \t y0:%d \tto\t x1:%d (%d) \t y1:%d (%d) \n",oldx,oldy,x,Program_run[a].togo,y,Program_run[a].temp);
    DrawVline(x,y,starty-y);
    oldx=x;
    oldy=y;
    if(Program_run[a].dwell){
      x=(int)(Program_run[a].dwell*scx)/100+oldx;
      u8g2.drawLine(oldx,oldy,x,y);
      //DBG Serial.printf("[LCD] ..Drawing line: x0:%d \t y0:%d \tto\t x1:%d (%d) \t y1:%d (%d)\n",oldx,oldy,x,Program_run[a].dwell,y,Program_run[a].temp);
      oldx=x;
    }
  }

  u8g2.sendBuffer();

  // If we are running - do the inverse part of graph
  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    int fullt,currt;
    float prop;
    fullt=(int)(Program_run_end-Program_run_start); // how long should program last
    currt=time(NULL)-Program_run_start;     // where are we now?
    prop=(float)currt/(float)fullt;   // current progress status
    u8g2.setDrawColor(2);
    DBG dbgLog(LOG_DEBUG,"[LCD] Redrawing box on graph width:%.2f fullt:%d currt:%d prop:%f\n",(float)((SCREEN_W-2)*prop),fullt,currt,prop);
    u8g2.drawBox(2,1,(int)((SCREEN_W-2)*prop),SCREEN_H-2);
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
  }
}



// Fist main view - basic running program information, status, time, start time, eta, temperatures
//
void LCD_display_mainv1(){
char msg[MAX_CHARS_PL];
uint16_t x=2,y=1;
uint8_t chh,chw,mch;
static uint8_t cnt=1;
struct tm timeinfo,*tmm;

  if(!Program_run_size) return; //  dont go in if no program loaded
  LCD_State=SCR_MAIN_VIEW;
  LCD_Main=MAIN_VIEW1;  // just in case...
  
  u8g2.clearBuffer();
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  strcmp(msg," ");
  chh=u8g2.getMaxCharHeight()+2;
  chw=u8g2.getMaxCharWidth();
  mch=floor(SCREEN_W-4/chw);  // how many chars per line...
  sprintf(msg,"%.*s",mch,Program_run_name);
  u8g2.drawBox(0,0, SCREEN_W, chh+1);
  u8g2.setDrawColor(0);
  y+=chh-1;
  u8g2.drawStr(x,y,msg);
  u8g2.setDrawColor(1);

  if(Program_run_state==PR_RUNNING){    // just some small animation for running program
    if(cnt==1) sprintf(msg,"%s",Prog_Run_Names[Program_run_state]);
    else if(cnt==2) sprintf(msg,"%s.",Prog_Run_Names[Program_run_state]);
    else if(cnt==3) sprintf(msg,"%s..",Prog_Run_Names[Program_run_state]);
    else if(cnt==4){
      sprintf(msg,"%s...",Prog_Run_Names[Program_run_state]);
      cnt=0;
    }
    cnt++;
  }else sprintf(msg,"%s",Prog_Run_Names[Program_run_state]);
  y+=chh;
  u8g2.drawStr(x+1,y,msg);

  // Program status & Clock
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W/2, chh+1);
  if(getLocalTime(&timeinfo)) strftime(msg, 20, "%T", &timeinfo);
  else sprintf(msg," No time");
  u8g2.drawStr(SCREEN_W/2+3,y,msg);

  // Start time
  u8g2.setFont(FONT6);
  chh=u8g2.getMaxCharHeight()+2;
  chw=u8g2.getMaxCharWidth();
  
  y+=chh;
  u8g2.drawFrame(0,y-chh, chw*7, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  sprintf(msg,"Start");
  u8g2.drawStr(x,y,msg);
  if(Program_run_start){
    tmm=localtime(&Program_run_start);
    sprintf(msg,"%2d-%02d %d:%02d:%02d",(tmm->tm_mon+1),tmm->tm_mday,tmm->tm_hour,tmm->tm_min,tmm->tm_sec);
    u8g2.drawStr(chw*8+3,y-1,msg);
  }
  
  //ETA time
  y+=chh;
  u8g2.drawFrame(0,y-chh, chw*7, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    sprintf(msg,"  ETA");
    u8g2.drawStr(x,y,msg);
    tmm=localtime(&Program_run_end);
    sprintf(msg,"%2d-%02d %d:%02d:%02d",(tmm->tm_mon+1),tmm->tm_mday,tmm->tm_hour,tmm->tm_min,tmm->tm_sec);
    u8g2.drawStr(chw*8+3,y-1,msg);
  }else{
    sprintf(msg,"  End");
    u8g2.drawStr(x,y,msg);
    if(Program_run_end){
      tmm=localtime(&Program_run_end);
      sprintf(msg,"%2d-%02d %d:%02d:%02d",(tmm->tm_mon+1),tmm->tm_mday,tmm->tm_hour,tmm->tm_min,tmm->tm_sec);
      u8g2.drawStr(chw*8+3,y-1,msg);
    }
  }

  // Temperatures - current, target, environment
  sprintf(msg,"Ct:%4.0fC St:%4.0fC Amb:%2.0fC",kiln_temp,set_temp,int_temp);
  u8g2.drawStr(x,y+=chh,msg);

  // Print Step number/all steps and if it's Run od Dwell, print proportional heat time of SSR and case temperature
  //if(Program_run_step>-1) sprintf(msg,"Stp:%d/%d%c Ht:%3.0f%%  Cse:%2dC",Program_run_step+1,Program_run_size,(temp_incr!=0)?'r':'d',(pid_out/Prefs[PRF_PID_WINDOW].value.uint16)*100.0);
  if(Program_run_step>-1) sprintf(msg,"Stp:%d/%d%c Ht:%3.0f%% Cse:%.0fC",Program_run_step+1,Program_run_size,(temp_incr!=0)?'r':'d',(pid_out/Prefs[PRF_PID_WINDOW].value.uint16)*100*PID_WINDOW_DIVIDER,case_temp);
  else sprintf(msg,"Stp:0/%d r/d Ht:0%% Cse:%.0fC",Program_run_size,case_temp);
  u8g2.drawStr(x,y+=chh-1,msg);
  
  u8g2.sendBuffer();
}


// Display main screens
//
void LCD_display_main_view(){
char sname[40];

  LCD_State=SCR_MAIN_VIEW;
  if(Program_run_size){
    if(LCD_Main==MAIN_VIEW1) LCD_display_mainv1();    // if any program loaded and view selected
    else if(LCD_Main==MAIN_VIEW2) LCD_display_mainv2();
    else if(LCD_Main==MAIN_VIEW3) LCD_display_mainv3();
  }else{
    u8g2.clearBuffer();          // clear the internal memory
    sprintf(sname,"Main screen %d",(int)LCD_Main);
    u8g2.setFont(FONT8);
    u8g2.drawStr(25,30,sname);
    u8g2.setFont(FONT7);
    sprintf(sname,"please load program");
    u8g2.drawStr(8,45,sname);
    u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
    u8g2.sendBuffer();
  }
}


// Display menu
//
void LCD_display_menu(){
char menu[MAX_CHARS_PL];
int m_startpos=LCD_Menu;
uint8_t chh,center=5;

  LCD_State=SCR_MENU;
  DBG dbgLog(LOG_DEBUG,"[LCD] Entering menu (%d) display: %s\n",LCD_Menu,Menu_Names[LCD_Menu]);
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT7);
  u8g2.setFontPosBaseline();
  chh=u8g2.getMaxCharHeight();
  center=floor((SCREEN_H-(chh+SCR_MENU_SPACE)*SCR_MENU_LINES)/2); // how much we have to move Y to be on the middle with all menu
  DBG dbgLog(LOG_DEBUG,"[LCD] In menu we can print %d lines, with %dpx space, and char height %d\n",SCR_MENU_LINES,SCR_MENU_SPACE,chh);
  
  if(LCD_Menu>SCR_MENU_MIDDLE) m_startpos=LCD_Menu-(SCR_MENU_MIDDLE-1); // if current menu pos > middle part of menu - start from LCD_Menu - SCR_MENU_MIDDLE-1
  else if(LCD_Menu<=SCR_MENU_MIDDLE) m_startpos=LCD_Menu-SCR_MENU_MIDDLE+1;  // if current menu pos < middle part - start
  DBG dbgLog(LOG_DEBUG,"[LCD] Start pos is %d, chosen position is %d, screen center is %d\n",m_startpos,LCD_Menu,center);
  
  for(int a=1; a<=SCR_MENU_LINES; a++){
    if(a==SCR_MENU_MIDDLE){   // reverse colors if we print middle part o menu
      u8g2.setDrawColor(1); /* color 1 for the box */
      DBG dbgLog(LOG_DEBUG,"[LCD] x0: %d, y0: %d, w: %d, h: %d\n",0, (a-1)*chh+SCR_MENU_SPACE+center, SCREEN_W , chh+SCR_MENU_SPACE);
      u8g2.drawBox(0, (a-1)*chh+SCR_MENU_SPACE+center+1, SCREEN_W , chh+SCR_MENU_SPACE);
      u8g2.setDrawColor(0);
    }
    if(m_startpos<0 || m_startpos>=M_END) u8g2.drawStr(15,(a*chh)+SCR_MENU_SPACE+center," ");  // just to add some top/bottom unselectable menu positions
    else{
      u8g2.drawStr(15,(a*chh)+SCR_MENU_SPACE+center,Menu_Names[m_startpos]);
    }
    u8g2.setDrawColor(1);
    m_startpos++;
  }
  
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display programs list
// action = direction of rotation
void LCD_display_programs(){
uint8_t chh,start_pos=0,max_lines=0;
uint16_t y=1,x=2;       // if we get bigger screen :)
int sel;
char msg[MAX_CHARS_PL];

  LCD_State=SCR_PROGRAM_LIST;

  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  chh=u8g2.getMaxCharHeight();
  max_lines=floor((SCREEN_H-y*2)/chh);
  u8g2.setFontPosBottom();

  if((sel=Find_selected_program())<0) return;
  
  if(sel>=floor(max_lines/2)){
    start_pos=sel-floor(max_lines/2);
    max_lines+=start_pos;
    if(max_lines>Programs_DIR_size) max_lines=Programs_DIR_size;
  }
  DBG dbgLog(LOG_DEBUG,"[LCD] Start pos:%d, sel_prg:%d, max_lines:%d\n",start_pos,sel,max_lines);
  
  for(start_pos; start_pos<max_lines && start_pos<Programs_DIR_size; start_pos++){
    if(Programs_DIR[start_pos].filesize<999) sprintf(msg,"%-15.15s %3db",Programs_DIR[start_pos].filename,Programs_DIR[start_pos].filesize);
    else sprintf(msg,"%-15.15s %2dkb",Programs_DIR[start_pos].filename,(int)(Programs_DIR[start_pos].filesize/1024));
    DBG dbgLog(LOG_INFO,"[LCD] Program list:%s: sel:%d\n",msg,Programs_DIR[start_pos].sel);
    if(Programs_DIR[start_pos].sel){
      u8g2.setDrawColor(1);
      u8g2.drawBox(0,y,SCREEN_W,chh);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x,y+=chh,msg);
      u8g2.setDrawColor(1);
    }else u8g2.drawStr(x,y+=chh,msg);
  }

  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
}


// Display question if delete program
//
void LCD_Display_program_delete(int dir/*=0*/, boolean pressed/*=0*/){
uint16_t x,y,h;
uint8_t chh,half;
static boolean yes=false;

  LCD_State=SCR_PROGRAM_DELETE;
  
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  u8g2.setFontMode(0);
  chh=u8g2.getMaxCharHeight()+2;
  x=5; y=30; h=SCREEN_H-y-x;
  u8g2.drawFrame(x,y,SCREEN_W-2*x,h);
  u8g2.setDrawColor(0);
  x++;y++;
  u8g2.drawBox(x,y,SCREEN_W-2*x,h-2);
  half=floor((SCREEN_W-2*x-2)/2);
  y+=chh+1; x+=4;
  u8g2.setDrawColor(1);
    
  if(!dir && !pressed) yes=false; // reset to default
  if(pressed)
    if(yes){
      if(Erase_program_file()){  // removed
        DBG Serial.println(" Program removed!");
        u8g2.drawStr(x,y+2,"File deleted!");
        u8g2.sendBuffer();
        u8g2.clearBuffer();
      }
      Cleanup_program();
      Generate_INDEX();
      delay(2500);
      LCD_display_programs(); // get back to program listing
      return;
    }else{
      LCD_Display_program_summary();
      return;
    }
    
  if(dir>0) yes=true;
  else if(dir<0) yes=false;

  u8g2.drawStr(x,y,"Delete program?");
  y+=chh;
  // Draw yes/no boxes
  x+=2;
  if(!yes){ // no delete
    u8g2.drawBox(7,y-chh+1,half,h/2-3);
    u8g2.setDrawColor(0);
    u8g2.drawStr(x+5,y,"No!");
    u8g2.setDrawColor(1);
    u8g2.drawStr(x+half+5,y,"Yes");
  }else{    // delete
    u8g2.drawStr(x+5,y,"No!");
    u8g2.drawBox(7+half,y-chh+1,half,h/2-3);
    u8g2.setDrawColor(0);
    u8g2.drawStr(x+half+5,y,"Yes");
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}


// Display full program, step by step
//
void LCD_Display_program_full(int dir/*=0*/){
uint8_t x=4,y=3,chh,lines;
static uint8_t pos=0;
char msg[MAX_CHARS_PL];
  
  LCD_State=SCR_PROGRAM_FULL;
  if(dir==0) pos=0;
  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()-1;
  lines=floor((SCREEN_H-2)/chh)-1;
  
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);

  if(dir>0 && (pos+lines-1)<Program_size) pos++;
  else if(dir<0 && pos>0) pos--;
  sprintf(msg,"nr Temp  Time  Dwell");
  u8g2.drawBox(0,1, SCREEN_W, chh);
  u8g2.setDrawColor(0);
  y+=chh;
  u8g2.drawStr(x,y-2,msg);
  u8g2.setDrawColor(1);
  
  for(int a=pos;a<Program_size && (a-pos)<lines;a++){
    sprintf(msg,"%d t:%4d T:%3d D:%3d",a+1,Program[a].temp,Program[a].togo,Program[a].dwell);
    u8g2.drawStr(x,y+=chh,msg);
  }
  u8g2.sendBuffer();
}


// Display single program info and options
// Dir - if dial rotated, load_prg - 0 = yes (if first time showing program (from program list)), 1 = no (rotating encoder), 2 = encoder pressed
//
void LCD_Display_program_summary(int dir,byte load_prg){
static int prog_menu=0;
char file_path[32];
uint8_t x=2,y=1,chh,err=0;
uint16_t sel;
char msg[125],rest[125];  // this should be 5 lines with 125 chars..  it should be malloc but ehh

  LCD_State=SCR_PROGRAM_SHOW;
// If the button was pressed - redirect to other screen
  if(load_prg==2){
    if(prog_menu==P_EXIT){ // exit - unload program, go to menu
      Cleanup_program();
      LCD_display_programs();
      return;
    }else if(prog_menu==P_SHOW){
      LCD_Display_program_full();
      return;
    }else if(prog_menu==P_LOAD){
      Load_program_to_run();
      LCD_display_main_view();
      return;
    }
  }
  
  u8g2.setFont(FONT6);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight();
  
  sel=Find_selected_program();    // get selected program
  DBG dbgLog(LOG_DEBUG,"[LCD] Show single program (dir %d, load_prg %d): %s\n",dir,load_prg,Programs_DIR[sel].filename);
  
  sprintf(file_path,"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
  DBG dbgLog(LOG_DEBUG,"[LCD]\tprogram path: %s\n",file_path);
  if(SPIFFS.exists(file_path)){
    u8g2.clearBuffer();
    
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
    u8g2.drawBox(0,0, SCREEN_W , chh+2);
    y+=chh;
    u8g2.setDrawColor(0);
    sprintf(msg,"Name: %s",Programs_DIR[sel].filename);
    return_LCD_string(msg,rest,-4);
    u8g2.drawStr(x,y,msg);
    u8g2.setDrawColor(1);

    y++; // add some space before description or error
       
    if(!load_prg)
      if(err=Load_program()){     // loading program - if >0 - failed with error - see description in PIDKiln.h
        u8g2.drawStr(x,y+=chh,"Program load failed!");
        sprintf(msg,"Error: %d",err);
        u8g2.drawStr(x,y+=chh,msg);
        u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
        u8g2.sendBuffer();
        return;
      }else prog_menu=0;

    strcpy(msg,Program_desc.c_str());   // Show program description - 3 lines max
    err=0; // reusage
    while(return_LCD_string(msg,rest,-4) && (err++<3)){
      u8g2.drawStr(x,y+=chh,msg);
      strcpy(msg,rest);
    }
    u8g2.drawStr(x,y+=chh,msg);
    
    y+=2; // +small space
    uint16_t max_t=0,total_t=0;     // calculate max temp and total time
    for(int a=0;a<Program_size;a++){
      if(Program[a].temp>max_t) max_t=Program[a].temp;
      total_t+=Program[a].togo+Program[a].dwell;
      DBG dbgLog(LOG_DEBUG,"[LCD] PRG: %d/%d Temp: %dC Time:%dm Dwell:%dm\n",a,Program_size,Program[a].temp,Program[a].togo,Program[a].dwell);
    }
    
    y=SCREEN_H-chh-1;
    sprintf(msg,"Tmax:%dC",max_t);
    DrawMenuEl(msg,y,2,1,0);
    sprintf(msg,"Time:%uh %dm",total_t/60,total_t%60);
    DrawMenuEl(msg,y,2,2,0);
    
    DBG dbgLog(LOG_DEBUG,"[LCD] Creating program menu prog_menu:%d dir:%d\n",prog_menu,dir);
    prog_menu+=dir;
    if(prog_menu>=Prog_Menu_Size) prog_menu=Prog_Menu_Size-1;
    else if(prog_menu<0) prog_menu=0;

    y=SCREEN_H;
    strcpy(msg,Prog_Menu_Names[0]);
    DrawMenuEl(msg,y,4,1,(prog_menu==0));
    strcpy(msg,Prog_Menu_Names[1]);
    DrawMenuEl(msg,y,4,2,(prog_menu==1));
    strcpy(msg,Prog_Menu_Names[2]);
    DrawMenuEl(msg,y,4,3,(prog_menu==2));
    strcpy(msg,Prog_Menu_Names[3]);
    DrawMenuEl(msg,y,4,4,(prog_menu==3));
      
    // If user wants to delete program - ask about it
    if(load_prg==2 && prog_menu==P_DELETE) LCD_Display_program_delete();
    else u8g2.sendBuffer();
  }
}


// Set manual, quick program step
// Dir - if dial rotated; pos - 0 = yes (if first time showing program (from program list)), 1 = no (rotating encoder), 2 = encoder pressed
//
void LCD_Display_quick_program(int dir,byte pos){
uint8_t x=2,y=1,chh;
static uint8_t what=0,ok=0,xm,ym;
static uint16_t qp[3];
char msg[100];

  LCD_State=SCR_QUICK_PROGRAM;
  
  if(pos==0 && dir==0){ // if we are here for the first time (not turn)
    what=ok=0;
    xm=ym=0;
    if(kiln_temp>0) qp[0]=kiln_temp;// target temperature
    else qp[0]=20;
    qp[1]=10;           // time to go
    qp[2]=10;           // dwell time
  }

  // Some actions without displaying anything
  if(pos==2){
    if(what==12){   // if button pressed on cancel
      LCD_display_menu();
      return;
    }else if(what==11){  // load program end exit
      DBG dbgLog(LOG_INFO,"[LCD] Replacing current program in memory - start");
      // We need to define program here
      Initialize_program_to_run();  // clear current program
      sprintf(msg,"Manually created quick program.");
      DBG dbgLog(LOG_DEBUG,"[LCD] Replacing current program in memory:%d \n",strlen(msg));
      Program_run_desc=(char *)MALLOC((strlen(msg)+1)*sizeof(char));
      strcpy(Program_run_desc,msg);
      sprintf(msg,"QuickProgram");
      DBG dbgLog(LOG_DEBUG,"[LCD] Replacing current program in memory:%d \n",strlen(msg));
      Program_run_name=(char *)MALLOC((strlen(msg)+1)*sizeof(char));
      strcpy(Program_run_name,msg);
      Update_program_step(0, qp[0], qp[1], qp[2]);
      Program_run_state=PR_READY;
      LCD_display_main_view();
      return;
    }else if(what==10){
      what=0; // user pushed edit
      pos=0;
    }
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()+1;

  // If this is just rotate
  if(what<4){ // temperature
    if(dir>0) qp[0]+=pow(10,what);
    else if(dir<0 && pow(10,what)<qp[0]) qp[0]-=pow(10,what);
  }
  else if(what<7){ // time
    if(dir>0) qp[1]+=pow(10,what-4);
    else if(dir<0 && pow(10,what-4)<qp[1]) qp[1]-=pow(10,what-4);
  }
  else if(what<10){ // dwell
    if(dir>0) qp[2]+=pow(10,what-7);
    else if(dir<0 && pow(10,what-7)<qp[2]) qp[2]-=pow(10,what-7);
  }
  else what+=dir; // rotate menu

  DBG dbgLog(LOG_DEBUG,"[LCD] Dir: %d What:%d Pos:%d\n",dir,what,pos);
  
  // If button pressed - cycle
  // 0-3 - temperature, 4-6 - time, 7-9 - dwell, 10 - cancel, 11 - load, 12 - back to edit
  if(pos==2) what++;
  if(what>12) what=0;
  
  // just in case = check values...
  if(qp[0]<1) qp[0]=1;
  else if(qp[0]>Prefs[PRF_MAX_TEMP].value.uint16) qp[0]=Prefs[PRF_MAX_TEMP].value.uint16;

  if(qp[1]<1) qp[1]=1;
  else if(qp[1]>999) qp[1]=999;

  if(qp[2]<1) qp[2]=1;
  else if(qp[2]>999) qp[2]=999;
  
  u8g2.drawFrame(0, 0, SCREEN_W, SCREEN_H);
  u8g2.drawBox(0, 0, SCREEN_W, chh+2);
  u8g2.setDrawColor(0);
  sprintf(msg,"Quick program");
  u8g2.drawStr(x,y+=chh,msg);
  u8g2.setDrawColor(1);
  y+=3;
  
  sprintf(msg,"Temperature");
  xm=u8g2.getStrWidth(msg);
  ym=y;
  u8g2.drawStr(x,y+=chh,msg);
  
  sprintf(msg," Time to go");
  if(u8g2.getStrWidth(msg)>xm) xm=u8g2.getStrWidth(msg);
  u8g2.drawStr(x,y+=chh,msg);
  
  sprintf(msg," Dwell time");
  if(u8g2.getStrWidth(msg)>xm) xm=u8g2.getStrWidth(msg);
  u8g2.drawStr(x,y+=chh,msg);

  DrawVline(xm+5,chh+1,SCREEN_H-2*chh-4);
  //u8g2.drawVLine(xm+5,chh+1,SCREEN_H-2*chh-4);
  xm+=10; y=ym;
  // Now we draw settable stuff
  sprintf(msg,"%04d C", qp[0]);
  if(what<4){ // if we are drawing detailed temperature
    u8g2.setCursor(xm, y+=chh);
    Draw_Marked_string(msg,3-what);
  }else u8g2.drawStr(xm,y+=chh,msg);

  sprintf(msg,"%03d min.", qp[1]);
  if(what>3 && what<7){ // if we are drawing detailed time
    u8g2.setCursor(xm, y+=chh);
    Draw_Marked_string(msg,6-what);    
  }else u8g2.drawStr(xm,y+=chh,msg);

  sprintf(msg,"%03d min.", qp[2]);
  if(what>6 && what<10){ // if we are drawing detailed time
    u8g2.setCursor(xm, y+=chh);
    Draw_Marked_string(msg,9-what);    
  }else u8g2.drawStr(xm,y+=chh,msg);

  y=SCREEN_H;
  sprintf(msg,"Edit");
  DrawMenuEl(msg,y,3,1,(what==10));
  sprintf(msg,"Use it");
  DrawMenuEl(msg,y,3,2,(what==11));
  sprintf(msg,"Cancel");
  DrawMenuEl(msg,y,3,3,(what==12));
  
  // and now submenu
  u8g2.sendBuffer();
}


// Display information screen
//
void LCD_Display_info(){
uint8_t chh,y=1,x=2;
char msg[MAX_CHARS_PL];
struct tm timeinfo;

  LCD_State=SCR_OTHER;
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT6);
  y+=chh=u8g2.getMaxCharHeight();

  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  if(WiFi.getMode()==WIFI_OFF){
    sprintf(msg,"WiFi status: disabled");
    u8g2.drawStr(x,y,msg);
  }else if(WiFi.getMode()==WIFI_STA){
    sprintf(msg,"WiFi mode: client");
    u8g2.drawStr(x,y,msg);
    sprintf(msg,"WiFi ssid: %s",Prefs[PRF_WIFI_SSID].value.str);
    u8g2.drawStr(x,y+=chh,msg);
    if(WiFi.isConnected()) sprintf(msg,"WiFi status: connected");
    else sprintf(msg,"WiFi status: disconnected");
    u8g2.drawStr(x,y+=chh,msg);
  }else if(WiFi.getMode()==WIFI_AP){
    sprintf(msg,"WiFi mode: AP");
    u8g2.drawStr(x,y,msg);
  }

  if(WiFi.getMode()){
    IPAddress lips;      
    Return_Current_IP(lips);
    sprintf(msg,"WiFi IP: %s",lips.toString().c_str());
    u8g2.drawStr(x,y+=chh,msg);
  }

  sprintf(msg,"MAC: %s",WiFi.macAddress().c_str());
  DBG dbgLog(LOG_DEBUG,"[LCD] Printing MAC address: %s\n",msg);
  u8g2.drawStr(x,y+=chh,msg);
  
  sprintf(msg,"Max prg. size: %d",MAX_Prog_File_Size);
  u8g2.drawStr(x,y+=chh,msg);
  
  if(getLocalTime(&timeinfo)) strftime(msg, MAX_CHARS_PL, "%F %T", &timeinfo);
  else sprintf(msg,"Failed to acquire time");
  u8g2.drawStr(x,y+=chh,msg);

  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display currently loaded preferences on LCD screen
//
void LCD_Display_prefs(int dir/*=0*/){
uint8_t x=4,y=3,chh,chw,lines;
static uint8_t pos=1;
char msg[MAX_CHARS_PL*2],rest[MAX_CHARS_PL];
  
  LCD_State=SCR_PREFERENCES;
  if(dir==0) pos=1;
  u8g2.clearBuffer();
  u8g2.setFont(FONT6);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()-1;
  lines=floor((SCREEN_H-2)/chh)-1;
  
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);

  if(dir>0 && (pos+lines-2)<PRF_end) pos++;
  else if(dir<0 && pos>1) pos--;
  sprintf(msg,"Prefs:%s",PREFS_FILE);  
  u8g2.drawBox(0,1, SCREEN_W, chh);
  u8g2.setDrawColor(0);
  y+=chh;
  u8g2.drawStr(x,y-2,msg);
  u8g2.setDrawColor(1);
  
  for(int a=pos;a<=PRF_end && (a-pos)<lines && y<SCREEN_H-5;a++){
    if(Prefs[a].type==STRING) sprintf(msg,"%s = %s",PrefsName[a],Prefs[a].value.str);
    else if(Prefs[a].type==VFLOAT) sprintf(msg,"%s = %.2f",PrefsName[a],Prefs[a].value.vfloat);
    else if(a>=PRF_end) sprintf(msg," ");
    else sprintf(msg,"%s = %d",PrefsName[a],Prefs[a].value.str);
    if(return_LCD_string(msg,rest,-4)){
      u8g2.drawStr(x,y+=chh,msg);
      y+=chh;
      if(y<SCREEN_H-4) u8g2.drawStr(x,y,rest);
    }else u8g2.drawStr(x,y+=chh,msg);
  }
  u8g2.sendBuffer();  
}


// Display about screen
//
void LCD_Display_about(){
  LCD_State=SCR_ABOUT;   // Update what are we showing on screen
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawStr(28,15,PVer);
  u8g2.drawStr(36,30,PDate);
  u8g2.setFont(FONT6);
  u8g2.drawStr(8,45,"Web page:");
  u8g2.drawStr(8,55,"adrian.siemieniak.net");
  u8g2.drawFrame(2,2,SCREEN_W-4,SCREEN_H-4);
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
}


// Restart the ESP device
//
void Restart_ESP(){
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawStr(25,30,PVer);
  u8g2.setFont(FONT7);
  u8g2.drawStr(18,45,"is restarting...");
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
  delay(1000);
  ESP.restart();
}


// Reconect to WiFi - if initial connection failed user may do it manually
//
void LCD_Reconect_WiFi(){
  char msg[MAX_CHARS_PL];

  LCD_State=SCR_OTHER;
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
  if(!Prefs[PRF_WIFI_MODE].value.uint8){
    strcpy(msg," WiFi is disabled!");
    load_msg(msg);
    return;
  }
  strcpy(msg,"Reconnecting WiFi");
  load_msg(msg);
  if(Setup_WiFi()){    // !!! Wifi connection FAILED
    DBG dbgLog(LOG_INFO,"[LCD] WiFi connection failed\n");
    strcpy(msg," WiFi con. failed ");
    load_msg(msg);
  }else{
    IPAddress lips;
      
    Return_Current_IP(lips);
    DBG Serial.println(lips); // Print ESP32 Local IP Address
    sprintf(msg," IP: %s",lips.toString().c_str());
    load_msg(msg);
  }
}






/* 
** Setup LCD screen 
**
*/
void Setup_LCD(void) {
  
  u8g2.begin();
  u8g2.setBusClock(900000);   // without lowering the clock (default 1Mhz) LCD gets some glitches
  
  u8g2.clearBuffer();         // clear the internal memory
  u8g2.setFont(FONT8);
  u8g2.drawStr(27,30,PVer);
  u8g2.drawStr(38,45,"starting...");
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(500);
  
}

// Simple functions to enable/disable SSR - for clarity, everything is separate
//
void Enable_SSR(){
  if(!SSR_On){
    digitalWrite(SSR1_RELAY_PIN, HIGH);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, HIGH);
#endif
    SSR_On=true;
  }
}

void Disable_SSR(){
  if(SSR_On){
    digitalWrite(SSR1_RELAY_PIN, LOW);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, LOW);
#endif
    SSR_On=false;
  }
}

void Enable_EMR(){
  digitalWrite(EMR_RELAY_PIN, HIGH);
}

void Disable_EMR(){
  digitalWrite(EMR_RELAY_PIN, LOW);
}

void print_bits(uint32_t raw){
    for (int i = 31; i >= 0; i--)
    {
        bool b = bitRead(raw, i);
        Serial.print(b);
    }

Serial.println();
}


// ThermocoupleA temperature readout
//
void Update_TemperatureA(){
  uint32_t raw;
  double kiln_tmp1;

  int i = 0; // Counter for arrays
  double internalTemp; // Read the internal temperature of the MAX31855.
  double rawTemp; // Read the temperature of the thermocouple. This temp is compensated for cold junction temperature.
  double thermocoupleVoltage= 0;
  double internalVoltage = 0;
  double correctedTemp = 0;
  double totalVoltage = 0;


  raw = ThermocoupleA.readRawData();
  //Serial.print("A");
  //print_bits(raw);

  if(!raw){ // probably MAX31855 not connected
    DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 for ThermocoupleA did not respond\n");
    ABORT_Program(PR_ERR_MAX31A_NC);
    return;
  }
  if(ThermocoupleA.detectThermocouple(raw) != MAX31855_THERMOCOUPLE_OK){
    switch (ThermocoupleA.detectThermocouple(raw))
    {
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA short to VCC\n");
        break;

      case MAX31855_THERMOCOUPLE_SHORT_TO_GND:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA short to GND\n");
        break;

      case MAX31855_THERMOCOUPLE_NOT_CONNECTED:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA not connected\n");
        break;

      default:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA unknown error, check spi cable\n");
        break;
    }
    if(TempA_errors<Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
      TempA_errors++;
      DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA had an error but we are still below grace threshold - continue. Error %d of %d\n",TempA_errors,Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);
    }else{
      ABORT_Program(PR_ERR_MAX31A_INT_ERR);
    }
    return;
  }

  kiln_tmp1 = ThermocoupleA.getColdJunctionTemperature(raw); 
  internalTemp = kiln_tmp1;
  int_temp = (int_temp+kiln_tmp1)/2;
  
  kiln_tmp1 = ThermocoupleA.getTemperature(raw);
  rawTemp = kiln_tmp1;

  // Steps 1 & 2. Subtract cold junction temperature from the raw thermocouple temperature.
          thermocoupleVoltage = (rawTemp - internalTemp)*0.041276;  // C * mv/C = mV
 
          // Step 3. Calculate the cold junction equivalent thermocouple voltage.
 
          if (internalTemp >= 0) { // For positive temperatures use appropriate NIST coefficients
             // Coefficients and equations available from http://srdata.nist.gov/its90/download/type_k.tab
 
             double c[] = {-0.176004136860E-01,  0.389212049750E-01,  0.185587700320E-04, -0.994575928740E-07,  0.318409457190E-09, -0.560728448890E-12,  0.560750590590E-15, -0.320207200030E-18,  0.971511471520E-22, -0.121047212750E-25};
 
             // Count the the number of coefficients. There are 10 coefficients for positive temperatures (plus three exponential coefficients),
             // but there are 11 coefficients for negative temperatures.
             int cLength = sizeof(c) / sizeof(c[0]);
 
             // Exponential coefficients. Only used for positive temperatures.
             double a0 =  0.118597600000E+00;
             double a1 = -0.118343200000E-03;
             double a2 =  0.126968600000E+03;
 
 
             // From NIST: E = sum(i=0 to n) c_i t^i + a0 exp(a1 (t - a2)^2), where E is the thermocouple voltage in mV and t is the temperature in degrees C.
             // In this case, E is the cold junction equivalent thermocouple voltage.
             // Alternative form: C0 + C1*internalTemp + C2*internalTemp^2 + C3*internalTemp^3 + ... + C10*internaltemp^10 + A0*e^(A1*(internalTemp - A2)^2)
             // This loop sums up the c_i t^i components.
             for (i = 0; i < cLength; i++) {
                internalVoltage += c[i] * pow(internalTemp, i);
             }
                // This section adds the a0 exp(a1 (t - a2)^2) components.
                internalVoltage += a0 * exp(a1 * pow((internalTemp - a2), 2));
          }
          else if (internalTemp < 0) { // for negative temperatures
             double c[] = {0.000000000000E+00,  0.394501280250E-01,  0.236223735980E-04, -0.328589067840E-06, -0.499048287770E-08, -0.675090591730E-10, -0.574103274280E-12, -0.310888728940E-14, -0.104516093650E-16, -0.198892668780E-19, -0.163226974860E-22};
             // Count the number of coefficients.
             int cLength = sizeof(c) / sizeof(c[0]);
 
             // Below 0 degrees Celsius, the NIST formula is simpler and has no exponential components: E = sum(i=0 to n) c_i t^i
             for (i = 0; i < cLength; i++) {
                internalVoltage += c[i] * pow(internalTemp, i) ;
             }
          }
 
          // Step 4. Add the cold junction equivalent thermocouple voltage calculated in step 3 to the thermocouple voltage calculated in step 2.
          totalVoltage = thermocoupleVoltage + internalVoltage;
 
          // Step 5. Use the result of step 4 and the NIST voltage-to-temperature (inverse) coefficients to calculate the cold junction compensated, linearized temperature value.
          // The equation is in the form correctedTemp = d_0 + d_1*E + d_2*E^2 + ... + d_n*E^n, where E is the totalVoltage in mV and correctedTemp is in degrees C.
          // NIST uses different coefficients for different temperature subranges: (-200 to 0C), (0 to 500C) and (500 to 1372C).
          if (totalVoltage < 0) { // Temperature is between -200 and 0C.
             double d[] = {0.0000000E+00, 2.5173462E+01, -1.1662878E+00, -1.0833638E+00, -8.9773540E-01, -3.7342377E-01, -8.6632643E-02, -1.0450598E-02, -5.1920577E-04, 0.0000000E+00};
 
             int dLength = sizeof(d) / sizeof(d[0]);
             for (i = 0; i < dLength; i++) {
                correctedTemp += d[i] * pow(totalVoltage, i);
             }
          }
          else if (totalVoltage < 20.644) { // Temperature is between 0C and 500C.
             double d[] = {0.000000E+00, 2.508355E+01, 7.860106E-02, -2.503131E-01, 8.315270E-02, -1.228034E-02, 9.804036E-04, -4.413030E-05, 1.057734E-06, -1.052755E-08};
             int dLength = sizeof(d) / sizeof(d[0]);
             for (i = 0; i < dLength; i++) {
                correctedTemp += d[i] * pow(totalVoltage, i);
             }
          }
          else if (totalVoltage < 54.886 ) { // Temperature is between 500C and 1372C.
             double d[] = {-1.318058E+02, 4.830222E+01, -1.646031E+00, 5.464731E-02, -9.650715E-04, 8.802193E-06, -3.110810E-08, 0.000000E+00, 0.000000E+00, 0.000000E+00};
             int dLength = sizeof(d) / sizeof(d[0]);
             for (i = 0; i < dLength; i++) {
                correctedTemp += d[i] * pow(totalVoltage, i);
             }
          } else { // NIST only has data for K-type thermocouples from -200C to +1372C. If the temperature is not in that range, set temp to impossible value.
             // Error handling should be improved.
             Serial.print("Temperature is out of range. This should never happen.");
             correctedTemp = NAN;
          }

  kiln_temp=(kiln_temp*0.5+correctedTemp*0.5);    // We try to make bigger hysteresis

  if(TempA_errors>0) TempA_errors=0;  // Lower errors count after proper readout
  
  DBG dbgLog(LOG_DEBUG, "[ADDONS] Temperature sensor A readout: Internal temp = %.1f \t Last temp = %.1f \t Average kiln temp = %.1f\n", int_temp, correctedTemp, kiln_temp);
}



#ifdef MAXCS2
// ThermocoupleB temperature readout
//
void Update_TemperatureB(){
uint32_t raw;
double case_tmp1;

  raw = ThermocoupleB.readRawData();
//Serial.print("B");
//print_bits(raw);
  if(!raw){ // probably MAX31855 not connected
    DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 for ThermocoupleB did not respond\n");
    ABORT_Program(PR_ERR_MAX31B_NC);
    return;
  }
  if(ThermocoupleB.detectThermocouple(raw) != MAX31855_THERMOCOUPLE_OK){
    switch (ThermocoupleB.detectThermocouple())
    {
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB short to VCC\n");
        break;

      case MAX31855_THERMOCOUPLE_SHORT_TO_GND:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB short to GND\n");
        break;

      case MAX31855_THERMOCOUPLE_NOT_CONNECTED:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB not connected\n");
        break;

      default:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB unknown error, check spi cable\n");
        break;
    }
    if(TempB_errors<Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
      TempB_errors++;
      DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB had an error but we are still below grace threshold - continue. Error %d of %d\n",TempB_errors,Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);
    }else{
      ABORT_Program(PR_ERR_MAX31B_INT_ERR);
    }
    return;
  }

  case_tmp1 = ThermocoupleB.getColdJunctionTemperature(raw); 
  int_temp = (int_temp+case_tmp1)/2;
  
  case_tmp1 = ThermocoupleB.getTemperature(raw);
  case_temp=(case_temp*0.8+case_tmp1*0.2);    // We try to make bigger hysteresis
  
  if(TempB_errors>0) TempB_errors--;  // Lower errors count after proper readout

  DBG dbgLog(LOG_DEBUG,"[ADDONS] Temperature sensor B readout: Internal temp = %.1f \t Last temp = %.1f \t Average case temp = %.1f\n", int_temp, case_tmp1, case_temp); 
}
#endif


// Measure current power usage - to be expanded
//
void Read_Energy_INPUT(){
double Irms;
static uint8_t cnt=0;
static uint32_t last=0;

#ifdef ENERGY_MON_PIN
  Irms = emon1.calcIrms(512);  // Calculate Irms only; 512 = number of samples (internaly ESP does 8 samples per measurement)
  if(Irms<ENERGY_IGNORE_VALUE){
    Energy_Wattage=0;
    return;   // In my case everything below 0,3A is just noise. Comparing to 10-30A we are going to use we can ignore it. Final readout is correct.  
  }
  Energy_Wattage=(uint16_t)(Energy_Wattage+Irms*EMERGY_MON_VOLTAGE)/2;  // just some small hysteresis
  if(last){
    uint16_t ttime;
    ttime=millis()-last;
    Energy_Usage+=(double)(Energy_Wattage*ttime)/3600000;  // W/h - 60*60*1000 (miliseconds)
  }
  last=millis();

  if(cnt++>20){
    DBG dbgLog(LOG_DEBUG,"[ADDONS] VCC is set:%d ; RAW Power: %.1fW, Raw current: %.2fA, Power global:%d W/h:%.6f\n",emon1.readVcc(),Irms*EMERGY_MON_VOLTAGE,Irms,Energy_Wattage,Energy_Usage);
    cnt=0;
  }

#else
  return;
#endif

}


// Power metter loop - read energy consumption
//
void Power_Loop(void * parameter){
  for(;;){
    Read_Energy_INPUT();  // current redout takes around 3-5ms - so we will do it 10 times a second.
    vTaskDelay( 100 / portTICK_PERIOD_MS );
  }
}


// Stops Alarm
//
void STOP_Alarm(){
  ALARM_countdown=0;
  digitalWrite(ALARM_PIN, LOW);
}
// Start Alarm
//
void START_Alarm(){
  if(!Prefs[PRF_ALARM_TIMEOUT].value.uint16) return;
  ALARM_countdown=Prefs[PRF_ALARM_TIMEOUT].value.uint16;
  digitalWrite(ALARM_PIN, HIGH);
}



void Setup_Addons(){
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR1_RELAY_PIN, OUTPUT);
  //pinMode(DRDY_PIN, INPUT);

#ifdef SSR2_RELAY_PIN
    pinMode(SSR2_RELAY_PIN, OUTPUT);
#endif

  pinMode(ALARM_PIN, OUTPUT);

  SSR_On=false;
  ThermocoupleA.begin(ESP32_SPI);
  
#ifdef MAXCS2
  ThermocoupleB.begin(ESP32_SPI);
#endif
#ifdef ENERGY_MON_PIN
  emon1.current(ENERGY_MON_PIN, ENERGY_MON_AMPS);
  xTaskCreatePinnedToCore(
              Power_Loop,      /* Task function. */
              "Power_metter",  /* String with name of task. */
              8192,            /* Stack size in bytes. */
              NULL,            /* Parameter passed as input of the task */
              2,               /* Priority of the task. */
              NULL,1);         /* Task handle. */
              
#endif
}

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
 else if(var=="WiFi_Mode3" && Prefs[PRF_WIFI_MODE].value.uint8==3) return "checked";
 else if(var=="WiFi_Retry_cnt") return String(Prefs[PRF_WIFI_RETRY_CNT].value.uint8);

 else if(var=="HTTP_Local_JS0" && Prefs[PRF_HTTP_JS_LOCAL].value.uint8==0) return "checked";
 else if(var=="HTTP_Local_JS1" && Prefs[PRF_HTTP_JS_LOCAL].value.uint8==1) return "checked";
 
 else if(var=="Auth_Username") return String(Prefs[PRF_AUTH_USER].value.str);
 else if(var=="Auth_Password") return String(Prefs[PRF_AUTH_PASS].value.str);
 
 else if(var=="NTP_Server1") return String(Prefs[PRF_NTPSERVER1].value.str);
 else if(var=="NTP_Server2") return String(Prefs[PRF_NTPSERVER2].value.str);
 else if(var=="NTP_Server3") return String(Prefs[PRF_NTPSERVER3].value.str);
 else if(var=="GMT_Offset_sec") return String(Prefs[PRF_GMT_OFFSET].value.int16);
 else if(var=="Daylight_Offset_sec") return String(Prefs[PRF_DAYLIGHT_OFFSET].value.int16);
 else if(var=="Initial_Date") return String(Prefs[PRF_INIT_DATE].value.str);
 else if(var=="Initial_Time") return String(Prefs[PRF_INIT_TIME].value.str);
 
 else if(var=="MIN_Temperature") return String(Prefs[PRF_MIN_TEMP].value.uint8);
 else if(var=="MAX_Temperature") return String(Prefs[PRF_MAX_TEMP].value.uint16);
 else if(var=="MAX_Housing_Temperature") return String(Prefs[PRF_MAX_HOUS_TEMP].value.uint16);
 else if(var=="Alarm_Timeout") return String(Prefs[PRF_ALARM_TIMEOUT].value.uint16);
 else if(var=="MAX31855_Error_Grace_Count") return String(Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);

 else if(var=="PID_Window") return String(Prefs[PRF_PID_WINDOW].value.uint16);
 else if(var=="PID_Kp") return String(Prefs[PRF_PID_KP].value.vfloat,4);
 else if(var=="PID_Ki") return String(Prefs[PRF_PID_KI].value.vfloat,4);
 else if(var=="PID_Kd") return String(Prefs[PRF_PID_KD].value.vfloat,4);
 else if(var=="PID_POE0" && Prefs[PRF_PID_POE].value.uint8==0) return "checked";
 else if(var=="PID_POE1" && Prefs[PRF_PID_POE].value.uint8==1) return "checked";
 else if(var=="PID_Temp_Threshold") return String(Prefs[PRF_PID_TEMP_THRESHOLD].value.int16);
 
 else if(var=="LOG_Window") return String(Prefs[PRF_LOG_WINDOW].value.uint16);
 else if(var=="LOG_Files_Limit") return String(Prefs[PRF_LOG_LIMIT].value.uint16);

 else if(var=="DBG_Serial0" && Prefs[PRF_DBG_SERIAL].value.uint8==0) return "selected";
 else if(var=="DBG_Serial1" && Prefs[PRF_DBG_SERIAL].value.uint8==1) return "selected";
 else if(var=="DBG_Syslog0" && Prefs[PRF_DBG_SYSLOG].value.uint8==0) return "selected";
 else if(var=="DBG_Syslog1" && Prefs[PRF_DBG_SYSLOG].value.uint8==1) return "selected";
 else if(var=="DBG_Syslog_Port") return String(Prefs[PRF_SYSLOG_PORT].value.uint16);
 else if(var=="DBG_Syslog_Srv") return String(Prefs[PRF_SYSLOG_SRV].value.str);

 else if(var=="ERRORS" && Errors){
  String out="<div class=error> There where errors: "+String(Errors)+"</div>";
  DBG dbgLog(LOG_ERR,"[HTTP] Errors pointer1:%p\n",Errors);
  free(Errors);Errors=NULL;
  return out;
 }
 return String();
}


// Template preprocessor for debug screen - debug_board.html
//
String Debug_ESP32(const String& var){

 //return String();

 // Main chip parameters
 //
 if(var=="CHIP_ID"){
   uint64_t chipid;
   char tmp[14];
   chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
   sprintf(tmp,"%04X%08X",(uint16_t)(chipid>>32),(uint32_t)chipid);
   DBG dbgLog(LOG_INFO,"[HTTP] Chip id: %s\n",tmp);
   return String(tmp);
 }else if (var=="SDK_VERSION") return String(ESP.getSdkVersion());
 else if (var=="CPU_FREQ") return String(ESP.getCpuFreqMHz());
 else if (var=="CHIP_REV") return String(ESP.getChipRevision());
 else if (var=="CHIP_MODEL") return String(ESP.getChipModel());
 else if (var=="CHIP_CORES") return String(ESP.getChipCores());
 else if (var=="CHIP_REVF") return String(REG_READ(EFUSE_BLK0_RDATA3_REG) >> 15, BIN);
 else if (var=="MAC_ADDRESS") return String(WiFi.macAddress());
 // SPI Flash RAM parameters
 //
 else if (var=="SFLASH_RAM"){
   float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
   DBG dbgLog(LOG_INFO,"[HTTP] flashChipSize: %f\n",flashChipSize);
   return String(flashChipSize);
 }else if (var=="FLASH_FREQ"){
   float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
   DBG dbgLog(LOG_INFO,"[HTTP] flashFreq: %f\n",flashFreq);
   return String(flashFreq);
 }else if (var=="SKETCH_SIZE"){
   float sketchSize = (float)ESP.getSketchSize() / 1024;
   DBG dbgLog(LOG_INFO,"[HTTP] sketchSize: %f\n",sketchSize);
   return String(sketchSize);
 }else if (var=="SKETCH_TOTAL"){ // There is an error in ESP framework that shows Total space as freespace - wrong name for the function
   float freeSketchSpace = (float)ESP.getFreeSketchSpace() / 1024;
   DBG dbgLog(LOG_INFO,"[HTTP] freeSketchSpace: %f\n",freeSketchSpace);
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
   DBG dbgLog(LOG_DEBUG,"flashChipMode: %s\n",mode.c_str());
   DBG dbgLog(LOG_DEBUG,"flashChipMode: %d\n",(byte)fMode);
   return String(mode);
 }
 // PSRAM parameters
 //
 else if (var=="TOTAL_PSRAM"){
   float psramSize = (float)ESP.getPsramSize() / 1024;
   DBG dbgLog(LOG_INFO,"[HTTP] psramSize: %f\n",psramSize);
   return String(psramSize);
 }else if (var=="FREE_PSRAM"){
   float freePsram = (float)ESP.getFreePsram() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] freePsram: %f\n",freePsram);
   return String(freePsram);
 }else if (var=="SMALEST_PSRAM"){
   float minFreePsram = (float)ESP.getMinFreePsram() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] minFreePsram: %f\n",minFreePsram);
   return String(minFreePsram);
 }else if (var=="LARGEST_PSRAM"){
   float maxAllocPsram = (float)ESP.getMaxAllocPsram() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] maxAllocPsram: %f\n",maxAllocPsram);
   return String(maxAllocPsram);
 }
 // RAM parameters
 //
 else if (var=="TOTAL_HEAP"){
   float heapSize = (float)ESP.getHeapSize() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] heapSize: %f\n",heapSize);
   return String(heapSize);
 }else if (var=="FREE_HEAP"){
   float freeHeap = (float)ESP.getFreeHeap() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] freeHeap: %f\n",freeHeap);
   return String(freeHeap);
 }else if (var=="SMALEST_HEAP"){
   float minFreeHeap = (float)ESP.getMinFreeHeap() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] minFreeHeap: %f\n",minFreeHeap);
   return String(minFreeHeap);
 }else if (var=="LARGEST_HEAP"){
   float maxAllocHeap = (float)ESP.getMaxAllocHeap() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] maxAllocHeap: %f\n",maxAllocHeap);
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
    DBG dbgLog(LOG_DEBUG,"[HTTP] Failed to open for writing index.html\n");
    return;
  }

  // Copy index head
  if(tmpf=SPIFFS.open("/prog_beg.txt", "r")){
    DBG dbgLog(LOG_DEBUG,"[HTTP] Head of index - copying...\n");
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
    DBG dbgLog(LOG_DEBUG,"[HTTP] End of index - copying...\n");
    tmp=tmpf.readString();
    //DBG Serial.println(tmp);
    index.print(tmp);
    tmpf.close();
  }
  index.flush();
  index.close(); 
}


// Generates kiln logs index - /logs.html
//
void Generate_LOGS_INDEX(){
String tmp;
template_str=String();

  File index,tmpf,file;

  if(Load_LOGS_Dir()) return; // can't load directory

  // Open index for writting
  tmp=String(LOG_Directory)+String("/index.html");
  if(!(index = SPIFFS.open(tmp.c_str(), "w"))){
    DBG dbgLog(LOG_DEBUG,"[HTTP] Failed to open for writing log/index.html\n");
    return;
  }

  // Copy index head
  if(tmpf=SPIFFS.open("/logs_beg.txt", "r")){
    DBG dbgLog(LOG_DEBUG,"[HTTP] Head of logs - copying...\n");
    tmp=tmpf.readString();
    //DBG Serial.println(tmp);
    index.print(tmp);
    tmpf.close();
  }

  for(uint16_t a=0; a<Logs_DIR_size; a++){
    //vTaskDelay(50);  // give some time to other processes - watchdog was tripping here often
    tmp=String(LOG_Directory)+String("/")+String(Logs_DIR[a].filename);
    
    template_str += "<td><a href=\""+tmp+"\" target=\"_blank\">"+String(Logs_DIR[a].filename)+"</a></td>";  
    template_str += "<td>"+String(Logs_DIR[a].filesize)+"</td>";
    if(tmp.lastIndexOf(".log")>0 && (file=SPIFFS.open(tmp.c_str(),"r"))){
      template_str += "<td>"+file.readStringUntil('\n')+"</td>";
      file.close();
    }else template_str += "<td>  </td>";
    template_str += "</tr>\n";
    //DBG Serial.print(template_str);
    index.print(template_str);
    template_str=String();
  }

  // Copy end of the index template
  if(tmpf=SPIFFS.open("/logs_end.txt", "r")){
    DBG dbgLog(LOG_DEBUG,"[HTTP] End of log index - copying...\n");
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
    if(Program_start_temp) tmp+=",y:"+String(Program_start_temp)+"},";
    else tmp+=",y:"+String(kiln_temp)+"},";
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
    if(CSVFile) return CSVFile.path();
    else return String("/logs/test.csv");
  }else if(var == "PROGRAM_NAME"){
    return String(Program_run_name);
  }else if(var == "CONFIG"){        // if we have log fie to show - show it on graph, otherwise show just program graph
    if(CSVFile) return String("config_with");
    else return String("config_without");
  }
  return String();
}


// Template preprocessor for main view - index.html, about and perhaps others
//
String About_parser(const String& var) {
String tmp;

  template_str=String();
  DBG dbgLog(LOG_DEBUG,var.c_str());
  if (var == "VERSION"){
    template_str+=PVer;
    template_str+=" ";
    template_str+=PDate;
  }
  
  DBG dbgLog(LOG_DEBUG,template_str.c_str());
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
     DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded file too large! Aborting\n");
     request->send(406, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_File_Size)+"<br><br><a href=/>Return to main view</a></body></html");
     abort=true;
     return;
  }
    
  if(!index){
    DBG dbgLog(LOG_INFO,"UploadStart: %s\n", tmp.c_str() );
    
    // Check if declared file size in header is not too large
    if(request->hasHeader("Content-Length")){
      AsyncWebHeader* h = request->getHeader("Content-Length");
      if(h->value().toInt()>MAX_Prog_File_Size){
        DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded file too large! Aborting\n");
        request->send(406, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_File_Size)+"<br><br><a href=/programs/>Return to programs view</a></body></html");
        abort=true;
        return;
      }
    }

    // Abort if filename is too long (otherwise esp will not write file to SPIFFS silently!)
    if(tmp.length()>MAX_FILENAME){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded filename is too large! Aborting\n");
      request->send(406, "text/html", "<html><body><h1>Filename is too long!</h1> Current limit is "+String(MAX_FILENAME)+"letters for directory and filename, so program name can be only "+String(MAX_PROGNAME)+" <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }

    char tmp_filename[MAX_PROGNAME];
    strcpy(tmp_filename,filename.c_str());
    // Abort if filename contains not allowed characters or trys to overwrite index.html
    if(!valid_filename(tmp_filename) || filename.compareTo("index.html")==0){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded filename containg bad characters! Aborting\n");
      request->send(406, "text/html", "<html><body><h1>Filename is bad!</h1> Filename contains not allowed characters - use letters, numbers and . _ signs <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }
    
    if (newFile) newFile.close();
    newFile = SPIFFS.open( tmp.c_str(), "w");
  }
  DBG dbgLog(LOG_DEBUG,"[HTTP] Next iteration of file upload...\n");
  for(size_t i=0; i<len; i++){
    if(!check_valid_chars(data[i])){ // Basic sanitization - check for allowed characters
      request->send(200, "text/html", "<html><body><h1>File contains not allowed character(s)!</h1> You can use all letters, numbers and basic symbols in ASCII code.<br><br><a href=/>Return to main view</a></body></html");
      delete_file(newFile);
      DBG dbgLog(LOG_ERR,"[HTTP] Basic program check failed!\n");
      abort=true;
      return;
    }else newFile.write(data[i]);
  }
  if(final){
    newFile.flush();
    DBG dbgLog(LOG_DEBUG,"[HTTP] UploadEnd: %s, %d B\n", newFile.name(), newFile.size());
    newFile.close();

    char fname[22];
    strcpy(fname,filename.c_str());
    DBG dbgLog(LOG_INFO,"[HTTP] Checking uploaded program structure: '%s'\n",fname); 
    uint8_t err=Load_program(fname);
    
    if(err){  // program did not validate correctly
      request->send(200, "text/html", "<html><body><h1>Program stucture is incorrect!</h1> Error code "+String(err)+".<br><br><a href=/programs/>Return to programs</a></body></html");
      delete_file(newFile=SPIFFS.open( tmp.c_str(), "r"));  // we need to open file again - to close it with already existing function
      DBG dbgLog(LOG_ERR,"[HTTP] Detailed program check failed!\n");
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
void POST_Handle_Delete(AsyncWebServerRequest *request){

  if(!_webAuth(request)) return;

  //Check if POST (but not File) parameter exists
  if(request->hasParam("prog_name", true) && request->hasParam("yes", true)){
    AsyncWebParameter* p = request->getParam("yes", true);
    if(p->value().compareTo(String("Yes!"))==0){ // yes user want's to delete program
      AsyncWebParameter* p = request->getParam("prog_name", true);
      char path[32];
      sprintf(path,"%s/%.*s",PRG_Directory,MAX_PROGNAME,p->value().c_str());
      DBG dbgLog(LOG_DEBUG,"[HTTP] Removing program: %s with fpath:%s\n",p->value().c_str(),path);
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
void GET_Handle_Delete(AsyncWebServerRequest *request){
File tmpf;
String tmps;

  DBG dbgLog(LOG_DEBUG,"[HTTP]  Request type: %d\n",request->method());
  DBG dbgLog(LOG_DEBUG,"[HTTP]  Request url: %s\n",request->url().c_str());

  if(!request->hasParam("prog_name") || !(tmpf=SPIFFS.open("/delete.html", "r")) ){  // if no program to delete - skip
    DBG dbgLog(LOG_ERR,"[HTTP]  Failed to find GET or open delete.html");
    request->redirect("/programs");
    return;
  }
 
  AsyncWebParameter* p = request->getParam("prog_name");
  
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server","ESP Async Web Server");

  DBG dbgLog(LOG_DEBUG,"[HTTP] Opened file is %s, program name is:%s\n",tmpf.path(),p->value().c_str());
  
  while(tmpf.available()){
    tmps=tmpf.readStringUntil('\n');
    tmps.replace("~PROGRAM_NAME~",p->value());
    //DBG Serial.printf("-:%s:\n",tmps.c_str());
    response->println(tmps.c_str());
  }

  tmpf.close();
  request->send(response);
}


// Load program from file to memory
//
void GET_Handle_Load(AsyncWebServerRequest *request){
char prname[MAX_FILENAME];

  if(!request->hasParam("prog_name")){  // if no program to load - skip
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to load program - no program name");
    request->redirect("/programs");
    return;
  }

  AsyncWebParameter* p = request->getParam("prog_name");
  strcpy(prname,p->value().c_str());
  if(!Load_program(prname)){
    request->redirect("/index.html");
    Load_program_to_run();
    return;
  }else{
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to load program - Load_program() failed");
    request->redirect("/programs");
    return;
  }
}


// Handle prefs upload
//
void handlePrefs(AsyncWebServerRequest *request){
boolean save=false;

  if(!_webAuth(request)) return;

  int params = request->params();
  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Prefs parser POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if(p->name().equalsIgnoreCase("save")){
        save=true;
        continue;
      }else if(p->name().equalsIgnoreCase("update")){
        continue;
      }else if(!Change_prefs_value(p->name(),p->value())){
        DBG dbgLog(LOG_DEBUG,"[HTTP]!!! We have post error for %s with '%s'\n",p->name().c_str(),p->value().c_str());
        // we have some errors add new field to error list
        if(Errors!=NULL){
          DBG dbgLog(LOG_DEBUG,"[HTTP] Realloc call of size %d\n",(strlen(Errors)+p->name().length()+3)*sizeof(char));
          Errors=(char *)REALLOC(Errors,(strlen(Errors)+p->name().length()+3)*sizeof(char));
          strcat(Errors," ");
          strcat(Errors,p->name().c_str());
          DBG dbgLog(LOG_DEBUG,"[HTTP] Errors now:%s\n",Errors);
        }else{
          DBG dbgLog(LOG_DEBUG,"[HTTP] Malloc call of size %d\n",(p->name().length()+3)*sizeof(char));
          Errors=strdup(p->name().c_str());
          DBG dbgLog(LOG_DEBUG,"[HTTP] Errors now:%s\n",Errors);
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

  if(!_webAuth(request)) return;

  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Index post parser: POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if(p->name().equalsIgnoreCase("prog_start")){ // start program
        if(Program_run_state==PR_PAUSED) RESUME_Program();
        else START_Program();
      }else if(p->name().equalsIgnoreCase("prog_pause") && Program_run_state==PR_RUNNING){
        PAUSE_Program();
      }else if(p->name().equalsIgnoreCase("prog_end") && (Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED)){
        END_Program();
      }else if(p->name().equalsIgnoreCase("prog_abort")){
        ABORT_Program(PR_ERR_USER_ABORT);
      }
    }
  }
  request->send(SPIFFS, "/index.html");
}


// Handle json vars for js
//
String handleVars(const String& var){
char str[30];
struct tm timeinfo, *tmm;

  if(var=="KILN_TEMP") return String(kiln_temp);
  else if(var=="SET_TEMP") return String(set_temp);
  else if(var=="ENV_TEMP") return String(int_temp);
  else if(var=="CASE_TEMP") return String(case_temp);
  else if(var=="HEAT_TIME") return String((pid_out/Prefs[PRF_PID_WINDOW].value.uint16)*100*PID_WINDOW_DIVIDER);
  else if(var=="TEMP_CHANGE") return String(temp_incr);
  else if(var=="STEP"){
    if(Program_run_state==PR_RUNNING){
      if(temp_incr==0) strcpy(str,"Dwell");
      else strcpy(str,"Running");
    }else strcpy(str,Prog_Run_Names[Program_run_state]);
    return String(Program_run_step+1)+" of "+String(Program_run_size)+" - "+String(str);
  }
  else if(var=="CURR_TIME"){
    if(getLocalTime(&timeinfo)){
      strftime (str, 29, "%F %T", &timeinfo);
      return String(str);
    }
  }else if(var=="PROG_START" && Program_run_start){
    tmm = localtime(&Program_run_start);
    strftime (str, 29, "%F %T", tmm);
    return String(str);
  }else if(var=="PROG_END" && Program_run_end){
    tmm = localtime(&Program_run_end);
    strftime (str, 29, "%F %T", tmm);
    return String(str);
  }else if(var=="LOG_FILE"){
    if(CSVFile) return CSVFile.path();
    else return String("/logs/test.csv");
  }if(var=="PROGRAM_STATUS") return String(Program_run_state);
  else return String(" "); 
}


// Function(s) writing LCD screenshot over WWW - not perfect - but working
//
char *screenshot;
void out(const char *s){
  strcat(screenshot,s);
}
void do_screenshot(AsyncWebServerRequest *request){

  screenshot=(char *)MALLOC(SCREEN_W*SCREEN_H*2*sizeof(char)+1);
  if(screenshot==NULL){
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to allocate memory for screenshot");
    request->send(500);
    return;
  }
  *screenshot='\0';
  //strcpy(screenshot,"");

  AsyncResponseStream *response = request->beginResponseStream("image/x-portable-bitmap");
  response->addHeader("Server","ESP Async Web Server");
  response->addHeader("Content-Disposition","attachment; filename=\"PIDKiln_screenshot.pbm\"");

  u8g2_WriteBufferPBM2(u8g2.getU8g2(),out);
  response->println(screenshot);
  free(screenshot); screenshot=NULL;
  
  request->send(response);
}


// Funnctin handling firmware upload/update (from https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino)
//
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
size_t content_len;
  
  if (!index){
    DBG dbgLog(LOG_INFO,"[HTTP] Beginning firmware update\n");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Please wait while the device reboots...");
    response->addHeader("Refresh", "20; url=/about.html");
    response->addHeader("Connection", "close");
    request->send(response);
    delay(1000);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      DBG Serial.println("[HTTP] Update complete");
      DBG Serial.flush();
      Restart_ESP();
    }
  }
}


// Basic WEB authentication
//
bool _webAuth(AsyncWebServerRequest *request){
    if(!request->authenticate(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str,NULL,false)) {
      request->requestAuthentication(NULL,false); // force basic auth
      return false;
    }
    return true;
}


/* 
** Setup Webserver screen 
**
*/
void SETUP_WebServer(void) {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  });

  server.serveStatic("/index.html", SPIFFS, "/index.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  server.serveStatic("/index_local.html", SPIFFS, "/index_local.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  
  server.on("/index.html", HTTP_POST, handleIndexPost);
  server.on("/index_local.html", HTTP_POST, handleIndexPost);
  
  server.on("/js/chart.js", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(SPIFFS, "/js/chart.js", String(), false, Chart_parser);
  });


  if(Prefs[PRF_HTTP_JS_LOCAL].value.str){
    server.on("/js/jquery-3.4.1.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/js/jquery-3.4.1.js", "text/javascript");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    });
    server.on("/js/Chart.2.9.3.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/js/Chart.2.9.3.bundle.min.js", "text/javascript");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    });
    server.on("/js/chartjs-datasource.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/js/chartjs-datasource.min.js", "text/javascript");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    });
  }else{
    server.on("/js/jquery-3.4.1.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->redirect(JS_JQUERY);
    });
    server.on("/js/Chart.2.9.3.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->redirect(JS_CHART);
    });
    server.on("/js/chartjs-datasource.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->redirect(JS_CHART_DS);
    });
  }


  server.on("/PIDKiln_vars.json", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(SPIFFS, "/PIDKiln_vars.json", "application/json", false, handleVars);
  });

  // Set default file for programs to index.html - because webserver was programmed by some Windows @%$$# :/
  server.serveStatic("/programs/", SPIFFS, "/programs/").setDefaultFile("index.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  server.serveStatic("/logs/", SPIFFS, "/logs/").setDefaultFile("index.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);

  // Upload new programs
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(200);
  }, handleUpload);

  server.on("/debug_board.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(SPIFFS, "/debug_board.html", String(), false, Debug_ESP32);
  });

  server.on("/preferences.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(SPIFFS, "/preferences.html", String(), false, Preferences_parser);
  });

  server.on("/preferences.html", HTTP_POST, handlePrefs);
  
  server.on("/delete.html", HTTP_GET, GET_Handle_Delete);
  
  server.on("/delete.html", HTTP_POST, POST_Handle_Delete);

  server.on("/load.html", HTTP_GET, GET_Handle_Load);

  server.on("/screenshot.html", HTTP_GET, do_screenshot);

  server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(SPIFFS, "/about.html", String(), false, About_parser);
  });

  server.on("/flash_firmware.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(SPIFFS, "/flash_firmware.html", String(), false, Preferences_parser);
  });
  
  server.on("/flash_firmware.html", HTTP_POST,
    [](AsyncWebServerRequest *request) { if(!_webAuth(request)) return; },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  
  // Serve some static data
  server.serveStatic("/icons/", SPIFFS, "/icons/");
  server.serveStatic("/js/", SPIFFS, "/js/");
  server.serveStatic("/css/", SPIFFS, "/css/");
  server.serveStatic(PREFS_FILE, SPIFFS, PREFS_FILE).setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  server.serveStatic("/favicon.ico", SPIFFS, "/icons/heat.png");

  server.onNotFound([](AsyncWebServerRequest *request){
    //request->send(404);
    request->send(404, "text/plain", "File not found");
  });

  // Start server
  server.begin();
}


// Stop/disable web server
//
void STOP_WebServer(){
  server.end();
}

// What to do if button pressed in menu
//
void pressed_menu(){
  switch(LCD_Menu){
    case M_SCR_MAIN_VIEW: LCD_display_main_view(); break;
    case M_LIST_PROGRAMS: LCD_display_programs(); break;
    case M_QUICK_PROGRAM: LCD_Display_quick_program(0,0); break;
    case M_INFORMATIONS: LCD_Display_info(); break;
    case M_PREFERENCES: LCD_Display_prefs(); break;
    case M_CONNECT_WIFI: LCD_Reconect_WiFi(); break;
    case M_ABOUT: LCD_Display_about(); break;
    case M_RESTART: Restart_ESP(); break;
    default: break;
  }
}



// Just redirect pressed button to separate functions
//
void button_Short_Press(){
  DBG dbgLog(LOG_DEBUG," Short press. Current view %d\n",(int)LCD_State);
  if(LCD_State==SCR_MENU) pressed_menu();
  else if(LCD_State==SCR_MAIN_VIEW && LCD_Main==MAIN_VIEW3) LCD_display_mainv3(0,2);
  else if(LCD_State==SCR_PROGRAM_LIST) LCD_Display_program_summary(0,0);
  else if(LCD_State==SCR_PROGRAM_SHOW) LCD_Display_program_summary(0,2);
  else if(LCD_State==SCR_PROGRAM_DELETE) LCD_Display_program_delete(0,1);
  else if(LCD_State==SCR_PROGRAM_FULL) LCD_Display_program_summary(0,1);
  else if(LCD_State==SCR_QUICK_PROGRAM) LCD_Display_quick_program(0,2);
  else LCD_display_menu();  // if pressed something else - go back to menu
}


// Handle long press on encoder
//
void button_Long_Press(){
  
  if(LCD_State==SCR_MENU){ // we are in menu - switch to main screen
    LCD_State=SCR_MAIN_VIEW;
    LCD_display_main_view();
    return;
  }else if(LCD_State==SCR_PROGRAM_SHOW){ // if we are showing program - go to program list
    LCD_State=SCR_PROGRAM_LIST;
    LCD_display_programs(); // LCD_Program is global
    return;
  }else{ // If we are in MAIN screen or Program list or in unknown area to to -> menu
    LCD_State=SCR_MENU; // switching to menu
    LCD_display_menu();
    return;    
  }
}


// Handle or rotation encoder input
//
void Rotate(){

// If we are in MAIN screen view
  if(LCD_State==SCR_MAIN_VIEW){
    if(LCD_Main==MAIN_VIEW3 && Program_run_size){ // if we are on third screen and program is loaded - let the screen 3 know about turn
      LCD_display_mainv3(encoderValue,1);
      return;
    }
    if(encoderValue<0){   // other screens just go...
      if(LCD_Main>MAIN_VIEW1) LCD_Main=(LCD_MAIN_View_enum)((int)LCD_Main-1);
      LCD_display_main_view();
      return;
    }else{
      if(LCD_Main<MAIN_end-1) LCD_Main=(LCD_MAIN_View_enum)((int)LCD_Main+1);
      LCD_display_main_view();
      return;
    }
  }else if(LCD_State==SCR_MENU){
    DBG dbgLog(LOG_DEBUG,"[INPUT] Rotate, SCR_MENU: Encoder turn: %d, Sizeof menu %d, Menu nr %d, \n",encoderValue, M_END, LCD_Menu);
    if(encoderValue<0){
      if(LCD_Menu>M_SCR_MAIN_VIEW) LCD_Menu=(LCD_SCR_MENU_Item_enum)((int)LCD_Menu-1);
    }else{
      if(LCD_Menu<M_END-1) LCD_Menu=(LCD_SCR_MENU_Item_enum)((int)LCD_Menu+1);
    }
    LCD_display_menu();
    return;
  }else if(LCD_State==SCR_PROGRAM_LIST){
    DBG dbgLog(LOG_DEBUG,"[INPUT] Rotate, PROGRAMS: Encoder turn: %d\n",encoderValue);
    rotate_selected_program(encoderValue);
    LCD_display_programs();
  }else if(LCD_State==SCR_PROGRAM_SHOW) LCD_Display_program_summary(encoderValue,1);
  else if(LCD_State==SCR_PROGRAM_DELETE) LCD_Display_program_delete(encoderValue,0);
  else if(LCD_State==SCR_PROGRAM_FULL) LCD_Display_program_full(encoderValue);
  else if(LCD_State==SCR_PREFERENCES) LCD_Display_prefs(encoderValue);
  else if(LCD_State==SCR_QUICK_PROGRAM) LCD_Display_quick_program(encoderValue,1);
}







// Main input loop task function
//
void Input_Loop(void * parameter) {

  for(;;){
    if(encoderButton){
      vTaskDelay( ENCODER_BUTTON_DELAY / portTICK_PERIOD_MS );
      if(digitalRead(ENCODER0_BUTTON)!=LOW){ // Button is still pressed - skip, perhaps it's a long press
        if(encoderButton+Long_Press>=millis()){ // quick press
          DBG dbgLog(LOG_DEBUG,"[INPUT] Button pressed %f seconds\n",(float)(millis()-encoderButton)/1000);
          button_Short_Press();
        }else{  // long press
          DBG dbgLog(LOG_DEBUG,"[INPUT] Button long pressed %f seconds\n",(float)(millis()-encoderButton)/1000);
          button_Long_Press();
        }
        encoderButton=0;
      }
    }else if(encoderValue!=0){
      vTaskDelay(ENCODER_ROTATE_DELAY / portTICK_PERIOD_MS);
      Rotate(); // encoderValue is global..
      DBG dbgLog(LOG_DEBUG,"[INPUT] Encoder rotated %d\n",encoderValue);
      encoderValue=0;
    }
    yield();
  }
}


// Interrupt parser for rotary encoder and it's button
//
void handleInterrupt() {

  if(digitalRead(ENCODER0_BUTTON)==LOW){
      encoderButton=millis();
  }else{ // Those two events can be simultaneous - but this is also ok, usually user does not press and turn

    /*int MSB = digitalRead(ENCODER0_PINA); //MSB = most significant bit
    int LSB = digitalRead(ENCODER0_PINB); //LSB = least significant bit
    int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
    int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue=1;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue=-1;

    lastEncoded = encoded; //store this value for next time*/

    //Такой костыль который заменяет вращение энкодера на кнопки вправо-влево
    if (digitalRead(ENCODER0_PINA)){
      encoderValue = 1;
    }
    if (digitalRead(ENCODER0_PINB)){
      encoderValue = -1;
    }

  }
}


// Setup all input pins and interrupts
//
void Setup_Input() {

  pinMode(ENCODER0_PINA, INPUT_PULLUP); 
  pinMode(ENCODER0_PINB, INPUT_PULLUP);
  pinMode(ENCODER0_BUTTON, INPUT_PULLUP);

  //digitalWrite(ENCODER0_PINA, HIGH); //turn pullup resistor on
  //digitalWrite(ENCODER0_PINB, HIGH); //turn pullup resistor on
  //digitalWrite(ENCODER0_BUTTON, HIGH); //turn pullup resistor on

  attachInterrupt(ENCODER0_PINA, handleInterrupt, FALLING);
  attachInterrupt(ENCODER0_PINB, handleInterrupt, FALLING);
  attachInterrupt(ENCODER0_BUTTON, handleInterrupt, FALLING);

  xTaskCreatePinnedToCore(
//  xTaskCreate(
              Input_Loop,       /* Task function. */
              "Input_loop",     /* String with name of task. */
              8192,             /* Stack size in bytes. */
              NULL,             /* Parameter passed as input of the task */
              1,                /* Priority of the task. */
              NULL,0);            /* Task handle. */
}

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
  if(Logs_DIR){
    free(Logs_DIR);
    Logs_DIR=NULL;
  }
  Logs_DIR=(DIRECTORY*)MALLOC(sizeof(DIRECTORY)*count);
  Logs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Logs_DIR_size<=count){    // now we do acctual loading into memory
    char tmp[32];
    uint8_t len2;

    vTaskDelay(20);
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

  if(Prefs[PRF_DBG_SYSLOG].value.uint8){
    DBG dbgLog(LOG_DEBUG,"[LOG] Trying to enable Syslog\n");
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
  }else DBG dbgLog(LOG_DEBUG,"[LOG] Syslog disabled\n");
}


void initSerial(){
  if(Prefs[PRF_DBG_SERIAL].value.uint8) Serial.begin(115200);
}

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

/*
** Preferences read/write routines
**
*/


// Find and change item in preferences
//
boolean Change_prefs_value(String item, String value){
  for(uint16_t a=0; a<PRF_end; a++){
    if(item.equalsIgnoreCase(String(PrefsName[a]))){  // we have found an matching prefs value
      if(Prefs[a].type==STRING){
        if(Prefs[a].value.str){
          free(Prefs[a].value.str);
          Prefs[a].value.str=NULL;
        }
        Prefs[a].value.str=strdup(value.c_str());
        DBG dbgLog(LOG_DEBUG,"[PREFS]  -> For %s saved STRING item value:%s type:%d\n",PrefsName[a],Prefs[a].value.str,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==UINT8){
        Prefs[a].value.uint8=(uint8_t)value.toInt();
        DBG dbgLog(LOG_DEBUG,"[PREFS]  -> For %s saved UINT8 item value:%d type:%d\n",PrefsName[a],Prefs[a].value.uint8,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==UINT16){
        Prefs[a].value.uint16=(uint16_t)value.toInt();
        DBG dbgLog(LOG_DEBUG,"[PREFS]  -> For %s saved UINT16 item value:%d type:%d\n",PrefsName[a],Prefs[a].value.uint16,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==INT16){
        Prefs[a].value.int16=(uint16_t)value.toInt();
        DBG dbgLog(LOG_DEBUG,"[PREFS]  -> For %s saved INT16 item value:%d type:%d\n",PrefsName[a],Prefs[a].value.int16,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==VFLOAT){
        Prefs[a].value.vfloat=(double)value.toDouble();
        DBG dbgLog(LOG_DEBUG,"[PREFS]  -> For %s saved VFLOAT item value:%f type:%d\n",PrefsName[a],Prefs[a].value.vfloat,(int)Prefs[a].type);
        return true;
      }
    }
  }
  return false;
}


// Save prefs to file
//
void Save_prefs(){
File prf;

  DBG dbgLog(LOG_INFO,"[PREFS] Writing prefs to file");
  if(prf=SPIFFS.open(PREFS_FILE,"w")){
    for(uint16_t a=1; a<PRF_end; a++){
      if(Prefs[a].type==STRING){
        prf.printf("%s = %s\n",PrefsName[a],Prefs[a].value.str);
      }else if(Prefs[a].type==UINT8){
        prf.printf("%s = %d\n",PrefsName[a],Prefs[a].value.uint8);
      }else if(Prefs[a].type==UINT16){
        prf.printf("%s = %d\n",PrefsName[a],Prefs[a].value.uint16);
      }else if(Prefs[a].type==INT16){
        prf.printf("%s = %d\n",PrefsName[a],Prefs[a].value.int16);
      }else if(Prefs[a].type==VFLOAT){
        prf.printf("%s = %.2f\n",PrefsName[a],Prefs[a].value.vfloat);
      }
    }
    prf.flush();
    prf.close();
  }
}


// Read prefs from file
//
void Load_prefs(){
File prf;
String line,item,value;
int pos=0;

  DBG dbgLog(LOG_INFO,"[PREFS] Loading prefs from file");
  if(prf=SPIFFS.open(PREFS_FILE,"r"))
    while(prf.available()){
      line=prf.readStringUntil('\n');
      line.trim();
      if(!line.length()) continue;         // empty line - skip it
      if(line.startsWith("#")) continue;   // skip every comment line
      if(pos=line.indexOf("#")){
        line=line.substring(0,pos);
        line.trim(); // trim again after removing comment
      }
      // Now we have a clean line - parse it
      if((pos=line.indexOf("="))>2){          // we need to have = sign - if not, ignore
        item=line.substring(0,pos-1);
        item.trim();
        value=line.substring(pos+1);
        value.trim();
        //DBG Serial.printf("[PREFS] Preference (=@%d) item: '%s' = '%s'\n",pos,item.c_str(),value.c_str());
        
        if(item.length()>2 && value.length()>0) Change_prefs_value(item,value);
      }
    }

    // For debuging only
    DBG dbgLog(LOG_DEBUG,"[PREFS] -=-=-= PREFS DISPLAY =-=-=-");
    for(uint16_t a=0; a<PRF_end; a++){
      if(Prefs[a].type==STRING) DBG dbgLog(LOG_DEBUG,"[PREFS] %d) '%s' = '%s'\t%d\n",a,PrefsName[a],Prefs[a].value.str,(int)Prefs[a].type);
      if(Prefs[a].type==UINT8) DBG dbgLog(LOG_DEBUG,"[PREFS] %d) '%s' = '%d'\t%d\n",a,PrefsName[a],Prefs[a].value.uint8,(int)Prefs[a].type);
      if(Prefs[a].type==UINT16) DBG dbgLog(LOG_DEBUG,"[PREFS] %d) '%s' = '%d'\t%d\n",a,PrefsName[a],Prefs[a].value.uint16,(int)Prefs[a].type);
      if(Prefs[a].type==INT16) DBG dbgLog(LOG_DEBUG,"[PREFS] %d) '%s' = '%d'\t%d\n",a,PrefsName[a],Prefs[a].value.int16,(int)Prefs[a].type);
    }
}


// This function is called whenever user changes preferences - perhaps we want to do something after that?
//
void Prefs_updated_hook(){

  // We have running program - update PID parameters
  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    KilnPID.SetTunings(Prefs[PRF_PID_KP].value.vfloat,Prefs[PRF_PID_KI].value.vfloat,Prefs[PRF_PID_KD].value.vfloat); // set actual PID parameters
  }
}

/* 
** Setup preferences for PIDKiln
**
*/
void Setup_prefs(void){
  char tmp[30];

  // Fill the preferences with default values - if there is such a need
  // DBG dbgLog(LOG_INFO,"[PREFS] Preference initialization");
  for(uint16_t a=1; a<PRF_end; a++)
    switch(a){
      case PRF_WIFI_SSID:
        Prefs[PRF_WIFI_SSID].type=STRING;
        Prefs[PRF_WIFI_SSID].value.str=strdup("");
        break;
      case PRF_WIFI_PASS:
        Prefs[PRF_WIFI_PASS].type=STRING;
        Prefs[PRF_WIFI_PASS].value.str=strdup("");
        break;
      case PRF_WIFI_MODE:
        Prefs[PRF_WIFI_MODE].type=UINT8;
        Prefs[PRF_WIFI_MODE].value.uint8=1;
        break;
      case PRF_WIFI_RETRY_CNT:
        Prefs[PRF_WIFI_RETRY_CNT].type=UINT8;
        Prefs[PRF_WIFI_RETRY_CNT].value.uint8=6;
        break;

      case PRF_HTTP_JS_LOCAL:
        Prefs[PRF_HTTP_JS_LOCAL].type=UINT8;
        Prefs[PRF_HTTP_JS_LOCAL].value.uint8=0;
        break;

      case PRF_AUTH_USER:
        Prefs[PRF_AUTH_USER].type=STRING;
        Prefs[PRF_AUTH_USER].value.str=strdup("admin");
        break;
      case PRF_AUTH_PASS:
        Prefs[PRF_AUTH_PASS].type=STRING;
        Prefs[PRF_AUTH_PASS].value.str=strdup("hotashell");
        break;

      case PRF_NTPSERVER1:
        Prefs[PRF_NTPSERVER1].type=STRING;
        Prefs[PRF_NTPSERVER1].value.str=strdup("pool.ntp.org");
        break;
      case PRF_NTPSERVER2:
        Prefs[PRF_NTPSERVER2].type=STRING;
        Prefs[PRF_NTPSERVER2].value.str=strdup("");
        break;
      case PRF_NTPSERVER3:
        Prefs[PRF_NTPSERVER3].type=STRING;
        Prefs[PRF_NTPSERVER3].value.str=strdup("");
        break;
      case PRF_GMT_OFFSET:
        Prefs[PRF_GMT_OFFSET].type=INT16;
        Prefs[PRF_GMT_OFFSET].value.int16=0;
        break;
      case PRF_DAYLIGHT_OFFSET:
        Prefs[PRF_DAYLIGHT_OFFSET].type=INT16;
        Prefs[PRF_DAYLIGHT_OFFSET].value.int16=0;
        break;
      case PRF_INIT_DATE:
        Prefs[PRF_INIT_DATE].type=STRING;
        Prefs[PRF_INIT_DATE].value.str=strdup("2021-11-18");
        break;
      case PRF_INIT_TIME:
        Prefs[PRF_INIT_TIME].type=STRING;
        Prefs[PRF_INIT_TIME].value.str=strdup("00:00:00");
        break;

      case PRF_PID_WINDOW:  // how often recalculate SSR on/off - 5 second window default
        Prefs[PRF_PID_WINDOW].type=UINT16;
        Prefs[PRF_PID_WINDOW].value.uint16=5000;
        break;
      case PRF_PID_KP:
        Prefs[PRF_PID_KP].type=VFLOAT;
        Prefs[PRF_PID_KP].value.vfloat=10;
        break;
      case PRF_PID_KI:
        Prefs[PRF_PID_KI].type=VFLOAT;
        Prefs[PRF_PID_KI].value.vfloat=0.2;
        break;
      case PRF_PID_KD:
        Prefs[PRF_PID_KD].type=VFLOAT;
        Prefs[PRF_PID_KD].value.vfloat=0.1;
        break;
      case PRF_PID_POE:   // it's actually boolean - but I did not want to create additional type - if we use  Proportional on Error (true) or Proportional on Measurement (false)
        Prefs[PRF_PID_POE].type=UINT8;
        Prefs[PRF_PID_POE].value.uint8=0;
        break;
      case PRF_PID_TEMP_THRESHOLD:  // allowed difference in temperature between set and current when controller will go in dwell mode
        Prefs[PRF_PID_TEMP_THRESHOLD].type=INT16;
        Prefs[PRF_PID_TEMP_THRESHOLD].value.int16=-1;
        break;

      case PRF_LOG_WINDOW:
        Prefs[PRF_LOG_WINDOW].type=UINT16;
        Prefs[PRF_LOG_WINDOW].value.uint16=30;
        break;
      case PRF_LOG_LIMIT:
        Prefs[PRF_LOG_LIMIT].type=UINT16;
        Prefs[PRF_LOG_LIMIT].value.uint16=40;
        break;

      case PRF_MIN_TEMP:
        Prefs[PRF_MIN_TEMP].type=UINT8;
        Prefs[PRF_MIN_TEMP].value.uint8=10;
        break;
      case PRF_MAX_TEMP:
        Prefs[PRF_MAX_TEMP].type=UINT16;
        Prefs[PRF_MAX_TEMP].value.uint16=1350;
        break;
      case PRF_MAX_HOUS_TEMP:
        Prefs[PRF_MAX_HOUS_TEMP].type=UINT16;
        Prefs[PRF_MAX_HOUS_TEMP].value.uint16=130;
        break;
      case PRF_THERMAL_RUN:
        Prefs[PRF_THERMAL_RUN].type=UINT16;
        Prefs[PRF_THERMAL_RUN].value.uint16=130;
        break;
      case PRF_ALARM_TIMEOUT:
        Prefs[PRF_ALARM_TIMEOUT].type=UINT16;
        Prefs[PRF_ALARM_TIMEOUT].value.uint16=0;
        break;
      case PRF_ERROR_GRACE_COUNT:
        Prefs[PRF_ERROR_GRACE_COUNT].type=UINT8;
        Prefs[PRF_ERROR_GRACE_COUNT].value.uint8=0;
        break;

      case PRF_DBG_SERIAL:
        Prefs[PRF_DBG_SERIAL].type=UINT8;
        Prefs[PRF_DBG_SERIAL].value.uint8=1;
        break;
      case PRF_DBG_SYSLOG:
        Prefs[PRF_DBG_SYSLOG].type=UINT8;
        Prefs[PRF_DBG_SYSLOG].value.uint8=0;
        break;
      case PRF_SYSLOG_SRV:
        Prefs[PRF_SYSLOG_SRV].type=STRING;
        Prefs[PRF_SYSLOG_SRV].value.str=strdup("");
        break;
      case PRF_SYSLOG_PORT:
        Prefs[PRF_SYSLOG_PORT].type=UINT16;
        Prefs[PRF_SYSLOG_PORT].value.uint16=0;
        break;

      default:
        break;
    }
}

/*
** Pidkiln program routines - main program functions and all PID magic
**
*/

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

  LCD_display_main_view();
  LCD_display_mainv1();
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
  if(Programs_DIR){
    free(Programs_DIR);
    Programs_DIR=NULL;
  }
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
void Program_recalculate_ETA(boolean dwell/*=false*/){

  Program_run_end=time(NULL);
  for(uint16_t a=Program_run_step; a<Program_run_size;a++){
    if(!dwell) Program_run_end+=Program_run[a].togo*60;
    else dwell=false;   // If we start counting with dwell=true - next step shoud start as a normal step, so dwell=fasle;
    Program_run_end+=Program_run[a].dwell*60;
  }
}


// This is functional called every second - if program is running. It calculates temperature grow, sets targets etc.
//
void Program_calculate_steps(boolean prg_start/*=false*/){
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
    //yield();
    vTaskDelay( 10 / portTICK_PERIOD_MS ); // This should enable to run other tasks on this core
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
