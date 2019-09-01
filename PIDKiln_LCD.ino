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
char menu[MAX_CHARS_PL];
int m_startpos=LCD_Menu;
uint8_t chh,center=5;

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
    if(m_startpos<0 || m_startpos>Menu_Size) u8g2.drawStr(15,(a*chh)+MENU_SPACE+center," ");  // just to add some top/bottom unselectable menu positions
    else{
      u8g2.drawStr(15,(a*chh)+MENU_SPACE+center,Menu_Names[m_startpos]);
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

  LCD_State=PROGRAM_LIST;

  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  chh=u8g2.getMaxCharHeight();
  max_lines=floor((SCREEN_H-y*2)/chh);
  u8g2.setFontPosBottom();

  if((sel=find_selected_program())<0) return;
  
  if(sel>=floor(max_lines/2)){
    start_pos=sel-floor(max_lines/2);
    max_lines+=start_pos;
    if(max_lines>Programs_DIR_size) max_lines=Programs_DIR_size;
  }
  DBG Serial.printf(" Start pos:%d, sel_prg:%d, max_lines:%d\n",start_pos,sel,max_lines);
  
  for(start_pos; start_pos<max_lines; start_pos++){
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

  LCD_State=PROGRAM_DELETE;
  
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
      if(erase_program()){  // removed
        DBG Serial.println(" Program removed!");
        u8g2.drawStr(x,y+2,"File deleted!");
        u8g2.sendBuffer();
        u8g2.clearBuffer();
      }
      cleanup_program();
      generate_index();
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
  y+=chh+1;
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
      
  for(byte a=0; a<Prog_Menu_Size; a++){
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


// Display single program info and options
// Dir - if dial rotated, load_prg - 0 = yes (if first time showing program (from program list)), 1 = no (rotating encoder), 2 = encoder pressed
//
void LCD_Display_program_summary(int dir,byte load_prg){
static int prog_menu=0;
char file_path[32];
uint8_t x=2,y=1,chh,err=0;
uint16_t sel;
char msg[125],rest[125];  // this should be 5 lines with 125 chars..  it should be malloc but ehh

  LCD_State=PROGRAM_SHOW;
// If the button was pressed - redirect to other screen
  if(load_prg==2){
    if(prog_menu==P_EXIT){ // exit - unload program, go to menu
      cleanup_program();
      LCD_display_programs();
      return;
    }else if(prog_menu==P_SHOW){
      
    }else if(prog_menu==P_LOAD){
    }
  }
  
  u8g2.setFont(FONT6);
  u8g2.setFontPosBottom();
  sel=find_selected_program();    // get selected program
  DBG Serial.printf("Show single program (dir %d, load_prg %d): %s\n",dir,load_prg,Programs_DIR[sel].filename);
  
  sprintf(file_path,"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
  DBG Serial.printf("\tprogram path: %s\n",file_path);
  if(SPIFFS.exists(file_path)){
    u8g2.clearBuffer();
    u8g2.setFont(FONT6);
    chh=u8g2.getMaxCharHeight();
    
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
      if(err=load_program()){     // loading program - if >0 - failed with error - see description in PIDKiln.h
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
void LCD_display_info(){
uint8_t chh,y,x=2;
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
  LCD_State=OTHER;    // uPDate what are we showing on screen
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
