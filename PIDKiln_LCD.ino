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

#define FONT6 u8g2_font_5x8_tr
#define FONT7 u8g2_font_6x10_tr
#define FONT8 u8g2_font_bitcasual_tr

// Other variables
//
static byte SCREEN_W=128;   // LCD screen width and height
static byte SCREEN_H=64;
static byte MENU_LINES=3;   // how many menu lines should be print
static byte MENU_SPACE=2;   // pixels spaces
static byte MENU_MIDDLE=2;  // middle of the menu, where choosing will be possible

#define LCD_RESET 4   // RST on LCD
#define LCD_CS 5      // RS on LCD
#define LCD_CLOCK 18  // E on LCD
#define LCD_DATA 23   // R/W on LCD

// You can switch hardware of software SPI interface to LCD. HW can be up to x10 faster - but requires special pins (and has some errors for me on 5V).
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ LCD_CLOCK, /* data=*/ LCD_DATA, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);
//U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);


// Write short messages during starting
void load_msg(char msg[20]){
  u8g2.setDrawColor(0);
  u8g2.drawBox(3, 31, 121, 22);
  u8g2.setDrawColor(1);
  u8g2.drawStr(10,45,msg);
  u8g2.sendBuffer();
}


// Display main screeen(s)
//
void LCD_display_main(){
char sname[40];

  LCD_State=MAIN_VIEW;
  u8g2.clearBuffer();          // clear the internal memory
  sprintf(sname,"Main screen %d",(int)LCD_Main);
  u8g2.setFont(FONT8);
  u8g2.drawStr(25,30,sname);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display menu
//
void LCD_display_menu(){
char menu[40];
int m_startpos=LCD_Menu;
byte chh,center=5;

  LCD_State=MENU;
  DBG Serial.printf("Entering menu (%d) display: %s\n",LCD_Menu,Menu_Names[LCD_Menu]);
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT7);
  u8g2.setFontPosBaseline();
  chh=u8g2.getMaxCharHeight();
  center=floor((SCREEN_H-(chh+MENU_SPACE)*MENU_LINES)/2); // how much we have to move Y to be on the middle with all menu
  DBG Serial.printf("In menu we can print %d lines, with %dpx space, and char height %d\n",MENU_LINES,MENU_SPACE,chh);
  
  if(LCD_Menu>MENU_MIDDLE) m_startpos=LCD_Menu-(MENU_MIDDLE-1); // if current menu pos > middle part of menu - start from LCD_Menu - MENU_MIDDLE-1
  else if(LCD_Menu<=MENU_MIDDLE) m_startpos=LCD_Menu-MENU_MIDDLE+1;  // if current menu pos < middle part - start
  DBG Serial.printf(" Start pos is %d, chosen position is %d, screen center is %d\n",m_startpos,LCD_Menu,center);
  
  for(int a=1; a<=MENU_LINES; a++){
    if(a==MENU_MIDDLE){   // reverse colors if we print middle part o menu
      u8g2.setDrawColor(1); /* color 1 for the box */
      DBG Serial.printf("x0: %d, y0: %d, w: %d, h: %d\n",0, (a-1)*chh+MENU_SPACE+center, SCREEN_W , chh+MENU_SPACE);
      u8g2.drawBox(0, (a-1)*chh+MENU_SPACE+center+1, SCREEN_W , chh+MENU_SPACE);
      u8g2.setDrawColor(0);
    }
    if(m_startpos<0 || m_startpos>Menu_Size) u8g2.drawStr(15,(a*chh)+MENU_SPACE+center,"....");
    else{
      u8g2.drawStr(15,(a*chh)+MENU_SPACE+center,Menu_Names[m_startpos]);
    }
    u8g2.setDrawColor(1);
    m_startpos++;
  }
  //u8g2.drawStr(25,30,Menu_Names[LCD_Menu]);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display programs list
//
void LCD_display_programs(int action){
static int sel_prg=0;  // this is a bit unsafe to remember program number, since it may change over HTTP - when someone will erase/upload. But doing it otherwise is to complex comparing to propability
byte chh,y,x=2,cnt=0;
char msg[40];
bool drawn=false;
String tmp_fname;
File file;

  LCD_State=PROGRAM_LIST;
  File dir = SPIFFS.open(PRG_DIRECTORY);
  if(!dir){
    DBG Serial.printf("Failed to open %s for listing\n",PRG_DIRECTORY);
    return;
  }
  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  y=chh=u8g2.getMaxCharHeight();
  u8g2.setFontPosBottom();

  DBG Serial.printf("Rotate programs. Action %d, sel_prg %d\n",action,sel_prg);
  if(sel_prg>0 && action==-1){
    sel_prg--;  // it's safe to scroll back, will see later if we can scroll forward
    action=0;
    DBG Serial.printf("\t\tmoved backward. Action %d, sel_prg %d\n",action,sel_prg);
  }
  
  while((file = dir.openNextFile()) && y<SCREEN_H) {  // if we are outside screen height - brake
    // List directory
    tmp_fname=file.name();
    
    tmp_fname=tmp_fname.substring(sizeof(PRG_DIRECTORY)); // remove path from the name
    if(tmp_fname=="index.html") continue;                 // If this is index.html - skip

    if(action==1 && cnt==sel_prg+1){
      DBG Serial.printf("\t\tmoved forward. Action %d, old sel_prg %d, new sel_prg %d\n",action,sel_prg,cnt);
      sel_prg=cnt;  // We moved forward in menu
      action=0;
    }
    
    sprintf(msg,"%-15s %3db",tmp_fname.c_str(),file.size());

    if(cnt==sel_prg && action!=1){
      u8g2.drawStr(x,y,msg);
      u8g2.drawFrame(0,y-chh,SCREEN_W,chh);
      Selected_Program=tmp_fname;     // Assign selected program to global var.
      drawn=true;
    }else u8g2.drawStr(x,y,msg);

    y+=chh;
    cnt++;
  }

  if(!drawn){ // If we haven't drawn a frame around program name because we moved outside the file list - do it now (this will happen always on the last file selected)
    y-=chh; // rewind :)
    u8g2.drawFrame(0,y-chh,SCREEN_W,chh);
    Selected_Program=tmp_fname;     // Assign selected program to global var.
  }
  u8g2.sendBuffer();
}


// Display single program info and options
//
void LCD_Display_program(){
char file_path[32];
byte x=0,y,chh,lnw=0,err=0;
char msg[40];

  LCD_State=PROGRAM_SHOW;
  DBG Serial.printf("Show single program: %s\n",Selected_Program.c_str());
  if(!Selected_Program.length()) return;  // program is not selected
  sprintf(file_path,"%s/%s",PRG_Directory,Selected_Program.c_str());
  DBG Serial.printf("\tprogram path: %s\n",file_path);
  if(SPIFFS.exists(file_path)){
    u8g2.clearBuffer();
    u8g2.setFont(FONT6);
    y=chh=u8g2.getMaxCharHeight();
    lnw=floor(SCREEN_W/u8g2.getMaxCharWidth()); // max chars in line
    sprintf(msg,"Name: %s",Selected_Program.c_str());
    u8g2.drawStr(x,y,msg);
    if(err=load_program()){     // loading program - if >0 - failed with error - see description in PIDKiln.h
      u8g2.drawStr(x,y+=chh,"Program load failed!");
      sprintf(msg,"Error: %d",err);
      u8g2.drawStr(x,y+=chh,msg);
      u8g2.sendBuffer();
      return;
    }
    u8g2.drawStr(x,y+=chh,"Description:");
    if(Program_desc.length()>lnw){    // If program description is longer then line width on display - cut it in two
      String desc=Program_desc.substring(0,lnw);
      u8g2.drawStr(x,y+=chh,desc.c_str());
      desc=Program_desc.substring(lnw,2*lnw);
      u8g2.drawStr(x,y+=chh,desc.c_str());
    }else u8g2.drawStr(x,y+=chh,Program_desc.c_str());
    y+=2; // small space
    unsigned int max_t=0,total_t=0;
    for(int a=0;a<Program_size;a++){
      if(Program[a].temp>max_t) max_t=Program[a].temp;
      total_t+=Program[a].togo+Program[a].dwell;
      DBG Serial.printf(" PRG: %d/%d Temp: %dC Time:%dm Dwell:%dm\n",a,Program_size,Program[a].temp,Program[a].togo,Program[a].dwell);
    }
    sprintf(msg," Max temperature: %dC",max_t);
    u8g2.drawStr(x,y+=chh,msg);
    sprintf(msg," Total time: %uh %dm",total_t/60,total_t%60);
    u8g2.drawStr(x,y+=chh,msg);
    u8g2.sendBuffer();
  }
}


// Display information screen
//
void LCD_display_info(){
byte chh,y,x=2;
char msg[40];

  LCD_State=OTHER;
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT6);
  y=chh=u8g2.getMaxCharHeight();
  
  sprintf(msg,"WiFi status: %d",WiFi.isConnected());
  u8g2.drawStr(x,y,msg);
  sprintf(msg,"WiFi ssid: %s",ssid);
  u8g2.drawStr(x,y+=chh,msg);
  if(WiFi.isConnected()){
    sprintf(msg,"WiFi IP: %s",WiFi.localIP().toString().c_str());
    u8g2.drawStr(x,y+=chh,msg);
  }
  sprintf(msg,"Max prg. size: %d",MAX_Prog_Size);
  u8g2.drawStr(x,y+=chh,msg);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display about screen
//
void LCD_display_about(){
  LCD_State=OTHER;    // update what are we showing on screen
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawStr(28,15,pver);
  u8g2.drawStr(36,30,pdate);
  u8g2.setFont(FONT6);
  u8g2.drawStr(8,45,"Web page:");
  u8g2.drawStr(8,55,"adrian.siemieniak.net");
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();
}

















// Setup LCD screen
//
void setup_lcd(void) {
  
  u8g2.begin();
  
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT8);
  u8g2.drawStr(25,30,pver);
  u8g2.drawStr(38,45,"starting...");
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(500);
}
