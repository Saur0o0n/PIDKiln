/*
** LCD Display components
**
*/
#include <U8g2lib.h>


#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define FONT4 u8g2_font_p01type_tr
#define FONT5 u8g2_font_micro_tr
#define FONT6 u8g2_font_5x8_tr
#define FONT7 u8g2_font_6x10_tr
#define FONT8 u8g2_font_bitcasual_tr

// Other variables
//
#define LCD_RESET 4   // RST on LCD
#define LCD_CS 5      // RS on LCD
#define LCD_CLOCK 18  // E on LCD
#define LCD_DATA 23   // R/W on LCD

// You can switch hardware of software SPI interface to LCD. HW can be up to x10 faster - but requires special pins (and has some errors for me on 5V).
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ LCD_CLOCK, /* data=*/ LCD_DATA, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);
//U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);


/*
** Core/main LCD functions
**
*/
void DrawVline(uint16_t x,uint16_t y,uint16_t h){

  DBG Serial.printf(" -> Draw V dots: x:%d\t y:%d\t h:%d\n",x,y,h);
  h+=y;
  for(uint16_t yy=y;yy<h;yy+=3) u8g2.drawPixel(x,yy);
}

// Second main view - program graph
//
void LCD_display_mainv2(){
uint16_t ttime=0,mxtemp=0,mxx=0,mxy=0,x,y,oldx,oldy,scx,scy,startx,starty;
char msg[MAX_CHARS_PL];

  if(!Program_run_size) return; //  dont go in if no program loaded
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
  if(mxtemp) scy=(int)((mxy*100)/mxtemp); // 1 celcius is scy pixel * 100

  DBG Serial.printf("Graph. mxx:%d mxy:%d ttime:%d mxtemp:%d scx:%d scy:%d\n",mxx,mxy,ttime,mxtemp,scx,scy);

  // Draw axies
  u8g2.drawHLine(1,SCREEN_H-1,mxx);
  u8g2.drawVLine(1,SCREEN_H-mxy,mxy);
  u8g2.drawHLine(0,SCREEN_H-mxy,3);
  sprintf(msg,"%dC",mxtemp);
  u8g2.drawStr(4,9,msg);
  
  startx=1;
  starty=SCREEN_H-1;  // 0.0 on screen is in left, upper corner - reverse it
  
  oldx=startx;
  oldy=starty;
  for(uint8_t a=0; a<Program_run_size; a++){
    x=startx;y=starty;      // in case of any fuckup - jest go to start point
    y=starty-(int)(Program_run[a].temp*scy)/100;
    x=(int)(Program_run[a].togo*scx)/100+oldx;
    u8g2.drawLine(oldx,oldy,x,y);
    DBG Serial.printf(".Drawing line: x0:%d \t y0:%d \tto\t x1:%d (%d) \t y1:%d (%d) \n",oldx,oldy,x,Program_run[a].togo,y,Program_run[a].temp);
    DrawVline(x,y,starty-y);
    oldx=x;
    oldy=y;
    if(Program_run[a].dwell){
      x=(int)(Program_run[a].dwell*scx)/100+oldx;
      u8g2.drawLine(oldx,oldy,x,y);
      DBG Serial.printf("..Drawing line: x0:%d \t y0:%d \tto\t x1:%d (%d) \t y1:%d (%d)\n",oldx,oldy,x,Program_run[a].dwell,y,Program_run[a].temp);
      oldx=x;
    }
  }

  u8g2.sendBuffer();
}



// Fist main view - basic running program informations, status, time, start time, eta, temperatures
//
void LCD_display_mainv1(){
char msg[MAX_CHARS_PL];
uint16_t x=2,y=1;
uint8_t chh,chw,mch;
struct tm timeinfo;

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
  mch=floor(SCREEN_W-4/chw); // how many chars per line...
  sprintf(msg,"%.*s",mch,Program_run_name);
  u8g2.drawBox(0,0, SCREEN_W, chh+1);
  u8g2.setDrawColor(0);
  y+=chh-1;
  u8g2.drawStr(x,y,msg);
  u8g2.setDrawColor(1);

  sprintf(msg,"%s",Prog_Run_Names[Program_run_state]);
  y+=chh;
  u8g2.drawStr(x+1,y,msg);

  // Program status & Clock
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W/2, chh+1);
  if(getLocalTime(&timeinfo)) sprintf(msg," %d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  else sprintf(msg," No time");
  u8g2.drawStr(SCREEN_W/2,y,msg);

  // Start time
  y+=chh;
  u8g2.drawFrame(0,y-chh, chw*8, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  sprintf(msg,"Start");
  u8g2.drawStr(x,y,msg);

  //ETA time
  y+=chh;
  u8g2.drawFrame(0,y-chh, chw*8, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  sprintf(msg,"ETA");
  u8g2.drawStr(x,y,msg);
  
  u8g2.sendBuffer();
}


// Display main screeens
//
void LCD_display_main_view(){
char sname[40];

  LCD_State=SCR_MAIN_VIEW;
  if(Program_run_size && LCD_Main==MAIN_VIEW1) LCD_display_mainv1();    // if any program loaded and view selected
  else if(Program_run_size && LCD_Main==MAIN_VIEW2) LCD_display_mainv2();
  else{
    u8g2.clearBuffer();          // clear the internal memory
    sprintf(sname,"Main screen %d",(int)LCD_Main);
    u8g2.setFont(FONT8);
    u8g2.drawStr(25,30,sname);
    u8g2.sendBuffer();          // transfer internal memory to the display 
  }
}


// Display menu
//
void LCD_display_menu(){
char menu[MAX_CHARS_PL];
int m_startpos=LCD_Menu;
uint8_t chh,center=5;

  LCD_State=SCR_MENU;
  DBG Serial.printf("Entering menu (%d) display: %s\n",LCD_Menu,Menu_Names[LCD_Menu]);
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT7);
  u8g2.setFontPosBaseline();
  chh=u8g2.getMaxCharHeight();
  center=floor((SCREEN_H-(chh+SCR_MENU_SPACE)*SCR_MENU_LINES)/2); // how much we have to move Y to be on the middle with all menu
  DBG Serial.printf("In menu we can print %d lines, with %dpx space, and char height %d\n",SCR_MENU_LINES,SCR_MENU_SPACE,chh);
  
  if(LCD_Menu>SCR_MENU_MIDDLE) m_startpos=LCD_Menu-(SCR_MENU_MIDDLE-1); // if current menu pos > middle part of menu - start from LCD_Menu - SCR_MENU_MIDDLE-1
  else if(LCD_Menu<=SCR_MENU_MIDDLE) m_startpos=LCD_Menu-SCR_MENU_MIDDLE+1;  // if current menu pos < middle part - start
  DBG Serial.printf(" Start pos is %d, chosen position is %d, screen center is %d\n",m_startpos,LCD_Menu,center);
  
  for(int a=1; a<=SCR_MENU_LINES; a++){
    if(a==SCR_MENU_MIDDLE){   // reverse colors if we print middle part o menu
      u8g2.setDrawColor(1); /* color 1 for the box */
      DBG Serial.printf("x0: %d, y0: %d, w: %d, h: %d\n",0, (a-1)*chh+SCR_MENU_SPACE+center, SCREEN_W , chh+SCR_MENU_SPACE);
      u8g2.drawBox(0, (a-1)*chh+SCR_MENU_SPACE+center+1, SCREEN_W , chh+SCR_MENU_SPACE);
      u8g2.setDrawColor(0);
    }
    if(m_startpos<0 || m_startpos>Menu_Size) u8g2.drawStr(15,(a*chh)+SCR_MENU_SPACE+center," ");  // just to add some top/bottom unselectable menu positions
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
  DBG Serial.printf(" Start pos:%d, sel_prg:%d, max_lines:%d\n",start_pos,sel,max_lines);
  
  for(start_pos; start_pos<max_lines && start_pos<Programs_DIR_size; start_pos++){
    if(Programs_DIR[start_pos].filesize<999) sprintf(msg,"%-15.15s %3db",Programs_DIR[start_pos].filename,Programs_DIR[start_pos].filesize);
    else sprintf(msg,"%-15.15s %2dkb",Programs_DIR[start_pos].filename,(int)(Programs_DIR[start_pos].filesize/1024));
    DBG Serial.printf(" Program list:%s: sel:%d\n",msg,Programs_DIR[start_pos].sel);
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
void LCD_Display_program_delete(int dir=0, boolean pressed=0){
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


// Draw program display menu
//
void LCD_Display_program_menu(byte pos=0){
uint8_t piece=0,y=0,chh;

  // Prepare menu for program
  piece=(SCREEN_W-1)/4;
  chh=u8g2.getMaxCharHeight();
  u8g2.setFontPosBottom();
  u8g2.setDrawColor(1);
  DBG Serial.printf("Creating prg menu. Size:%d Piece:%d Chh:%d\n",Prog_Menu_Size,piece,chh);
      
  for(uint8_t a=0; a<Prog_Menu_Size; a++){
    if(a==pos){
      if(pos==Prog_Menu_Size-1) u8g2.drawBox(piece*a,SCREEN_H-chh-2, SCREEN_W-piece*a , chh+1); // dirty hack - because screen has 127 pixels, not 126 - it's not possible to divide it by 4 - so it wont go till the end of the screen
      else u8g2.drawBox(piece*a,SCREEN_H-chh-2, piece+1 , chh+1);
      u8g2.setDrawColor(0);
      u8g2.drawStr(piece*a+2,SCREEN_H-1,Prog_Menu_Names[a]);
      u8g2.setDrawColor(1);
    }else{
      u8g2.drawStr(piece*a+2,SCREEN_H-1,Prog_Menu_Names[a]);
    }
    //DBG Serial.printf("Pos x:%d y:%d sel:%d text:%s\n",piece*a,SCREEN_H-1,pos,Prog_Menu_Names[a]);
  }

}


// Display full program, step by step
//
void LCD_Display_program_full(int dir=0){
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
  DBG Serial.printf("Show single program (dir %d, load_prg %d): %s\n",dir,load_prg,Programs_DIR[sel].filename);
  
  sprintf(file_path,"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
  DBG Serial.printf("\tprogram path: %s\n",file_path);
  if(SPIFFS.exists(file_path)){
    u8g2.clearBuffer();
    
    u8g2.setDrawColor(1); /* color 1 for the box */
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
      DBG Serial.printf(" PRG: %d/%d Temp: %dC Time:%dm Dwell:%dm\n",a,Program_size,Program[a].temp,Program[a].togo,Program[a].dwell);
    }
    sprintf(msg,"Tmax:%dC",max_t);
    u8g2.drawStr(x,y=SCREEN_H-chh-2,msg);
    sprintf(msg,"Time:%uh %dm",total_t/60,total_t%60);
    u8g2.drawStr((int)SCREEN_W/2,y,msg);
    u8g2.drawFrame(0,y-chh-1,SCREEN_W/2-1,chh+2);
    u8g2.drawFrame(SCREEN_W/2-1,y-chh-1,SCREEN_W,chh+2);
    u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);

    DBG Serial.printf(" Creating program menu prog_menu:%d dir:%d\n",prog_menu,dir);
    prog_menu+=dir;
    if(prog_menu>=Prog_Menu_Size) prog_menu=Prog_Menu_Size-1;
    else if(prog_menu<0) prog_menu=0;
    LCD_Display_program_menu(prog_menu);
    
    // If user wants to delete program - ask about it
    if(load_prg==2 && prog_menu==P_DELETE) LCD_Display_program_delete();
    else u8g2.sendBuffer();
  }
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
  sprintf(msg,"WiFi status: %d",WiFi.isConnected());
  u8g2.drawStr(x,y,msg);
  sprintf(msg,"WiFi ssid: %s",Prefs[PRF_WIFI_SSID].value.str);
  u8g2.drawStr(x,y+=chh,msg);
  if(WiFi.isConnected()){
    sprintf(msg,"WiFi IP: %s",WiFi.localIP().toString().c_str());
    u8g2.drawStr(x,y+=chh,msg);
  }
  sprintf(msg,"Max prg. size: %d",MAX_Prog_File_Size);
  u8g2.drawStr(x,y+=chh,msg);
  
  if(getLocalTime(&timeinfo)) sprintf(msg,"Date:%4d-%02d-%02d %d:%02d:%02d",(1900+timeinfo.tm_year),(timeinfo.tm_mon+1),timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  else sprintf(msg,"Failed to acquire time");
  u8g2.drawStr(x,y+=chh,msg);

  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display currently loaded preferences on LCD screen
//
void LCD_Display_prefs(int dir=0){
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
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();
}




/*
** Helping functions
**
*/

// Cut string for LCD width, return 1 if there's something left
// (input string, rest to output, screen width modificator)
boolean return_LCD_string(char* msg,char* rest,int mod){
uint16_t chh,lnw;
char out[MAX_CHARS_PL]; 

  chh=u8g2.getMaxCharHeight();
  //DBG Serial.printf("Line cut: Got:%s\n",msg);
  lnw=floor((SCREEN_W+mod)/u8g2.getMaxCharWidth())-1; // max chars in line
  if(strlen(msg)<=lnw){
    rest[0]='\0';
    //DBG Serial.printf("Line cut: line shorter then %d - skipping\n",lnw);
    return false;
  }
  strncpy(rest,msg+lnw+1,strlen(msg)-lnw);
  rest[strlen(msg)-lnw]='\0';
  msg[lnw+1]='\0';
  //DBG Serial.printf("Line cut. Returning msg:'%s' and rest:'%s'\n",msg,rest);
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






/* 
** Setup LCD screen 
**
*/
void setup_lcd(void) {
  
  u8g2.begin();
  
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT8);
  u8g2.drawStr(27,30,PVer);
  u8g2.drawStr(38,45,"starting...");
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(500);
}
