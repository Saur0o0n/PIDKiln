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

  lcd_state=MAIN_VIEW;
  u8g2.clearBuffer();          // clear the internal memory
  sprintf(sname,"Main screen %d",(int)lcd_main);
  u8g2.setFont(u8g2_font_courB08_tf);
  u8g2.drawStr(25,30,sname);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display menu
//
void LCD_display_menu(){
char menu[40];
int m_startpos=lcd_menu;
byte chh,center=5;


  DBG Serial.printf("Entering menu (%d) display: %s\n",lcd_menu,menu_names[lcd_menu]);
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_courB08_tf);
  chh=u8g2.getMaxCharHeight();
  center=floor((SCREEN_H-(chh+MENU_SPACE)*MENU_LINES)/2); // how much we have to move Y to be on the middle with all menu
  DBG Serial.printf("In menu we can print %d lines, with %dpx space, and char height %d\n",MENU_LINES,MENU_SPACE,chh);
  
  if(lcd_menu>MENU_MIDDLE) m_startpos=lcd_menu-(MENU_MIDDLE-1); // if current menu pos > middle part of menu - start from lcd_menu - MENU_MIDDLE-1
  else if(lcd_menu<=MENU_MIDDLE) m_startpos=lcd_menu-MENU_MIDDLE+1;  // if current menu pos < middle part - start
  DBG Serial.printf(" Start pos is %d, choosen position is %d, screen center is %d\n",m_startpos,lcd_menu,center);
  
  for(int a=1; a<=MENU_LINES; a++){
    if(a==MENU_MIDDLE){   // reverse colors if we print middle part o menu
      u8g2.setDrawColor(1); /* color 1 for the box */
      DBG Serial.printf("x0: %d, y0: %d, w: %d, h: %d\n",0, (a-1)*chh+MENU_SPACE+center, SCREEN_W , chh+MENU_SPACE);
      u8g2.drawBox(0, (a-1)*chh+MENU_SPACE+center+1, SCREEN_W , chh+MENU_SPACE+1);
      u8g2.setDrawColor(0);
    }
    if(m_startpos<0 || m_startpos>menu_size) u8g2.drawStr(15,(a*chh)+MENU_SPACE+center,"....");
    else{
      u8g2.drawStr(15,(a*chh)+MENU_SPACE+center,menu_names[m_startpos]);
    }
    u8g2.setDrawColor(1);
    m_startpos++;
  }
  //u8g2.drawStr(25,30,menu_names[lcd_menu]);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display programs list
//
void LCD_display_programs(){
//lcd_program

  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_profont10_tf);
  u8g2.drawStr(25,30,"aa");
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display information screen
//
void LCD_display_info(){
byte chh,y,x=2;
char msg[40];

  lcd_state=OTHER;
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_profont10_tf);
  y=chh=u8g2.getMaxCharHeight();
  
  sprintf(msg,"WiFi status: %d",WiFi.isConnected());
  u8g2.drawStr(x,y,msg);
  sprintf(msg,"WiFi ssid: %s",ssid);
  u8g2.drawStr(x,y+=chh,msg);
  if(WiFi.isConnected()){
    sprintf(msg,"WiFi IP: %s",WiFi.localIP().toString().c_str());
    u8g2.drawStr(x,y+=chh,msg);
  }
  sprintf(msg,"Max prg. size: %d",max_prog_size);
  u8g2.drawStr(x,y+=chh,msg);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display about screen
//
void LCD_display_about(){
  lcd_state=OTHER;    // update what are we showing on screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_courB08_tf);
  u8g2.drawStr(28,15,pver);
  u8g2.drawStr(36,30,pdate);
  u8g2.setFont(u8g2_font_profont10_tf);
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
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(25,30,pver);
  u8g2.drawStr(38,45,"starting...");
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(500);
}
