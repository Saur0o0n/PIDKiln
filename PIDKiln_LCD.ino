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

// You can switch hardware or software SPI interface to LCD. HW can be up to x10 faster - but requires special pins (and has some errors for me on 5V).
//U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ LCD_CLOCK, /* data=*/ LCD_DATA, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);


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
void LCD_display_mainv3(int dir=0, byte ctrl=0){
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
