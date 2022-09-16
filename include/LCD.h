/*
** LCD Display components
**
*/
#include <Arduino.h>

// Defines
#define LCD_RESET 4   // RST on LCD
#define LCD_CS 5      // RS on LCD
#define LCD_CLOCK 18  // E on LCD
#define LCD_DATA 23   // R/W on LCD

#define SCREEN_W 128   // LCD screen width and height
#define SCREEN_H 64
#define MAX_CHARS_PL SCREEN_W/3  // char can have min. 3 points on screen

//Typedefs
typedef enum { // program menu positions
  P_EXIT,
  P_SHOW,
  P_LOAD,
  P_DELETE,
  P_end
} LCD_PSCR_MENU_Item_enum;



//PFP
void Test_Setup_LCD();

boolean return_LCD_string(char* msg,char* rest, int mod, uint16_t screen_w=SCREEN_W);
void load_msg(char msg[MAX_CHARS_PL]);
void DrawVline(uint16_t x,uint16_t y,uint16_t h);
void Draw_Marked_string(char *str,uint8_t pos);
void DrawMenuEl(char *msg, uint16_t y, uint8_t cnt, uint8_t el, boolean sel);

void LCD_display_mainv3(int dir=0, byte ctrl=0);
void LCD_display_mainv2();
void LCD_display_mainv1();
void LCD_display_main_view();
void LCD_display_menu();
void LCD_display_programs();
void LCD_Display_program_delete(int dir=0, boolean pressed=0);
void LCD_Display_program_full(int dir=0);
void LCD_Display_program_summary(int dir=0,byte load_prg=0);
void LCD_Display_quick_program(int dir=0,byte pos=0);
void LCD_Display_info();
void LCD_Display_prefs(int dir=0);
void LCD_Display_about();

void Restart_ESP();
void LCD_Reconect_WiFi();
void Setup_LCD(void);
