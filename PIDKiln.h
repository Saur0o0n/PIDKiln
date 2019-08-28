
// Global value of LCD screen/menu and menu pos
typedef enum {
  MAIN_VIEW,    // group of main screens showing runnin program
  MENU,         // menu
  PROGRAM_LIST, // list of all programs
  PROGRAM_SHOW, // showing program conent
  OTHER         // some other screens like about that are stateless
} LCD_State;

typedef enum { // different main screens
  MAIN_VIEW1,
  MAIN_VIEW2,
  MAIN_VIEW3,
  MAIN_end
} LCD_MAIN_View;

typedef enum { // menu positions
  M_MAIN_VIEW,
  M_LIST_PROGRAMS,
  M_INFORMATIONS,
  M_ABOUT,
  M_end
} LCD_MENU_Item;

LCD_State lcd_state=MAIN_VIEW;      // global variable to keep track on where we are in LCD screen
LCD_MAIN_View lcd_main=MAIN_VIEW1;  // main screen has some views - where are we
LCD_MENU_Item lcd_menu=M_MAIN_VIEW; // menu items

const char *menu_names[] = {"1) Main view","2) List programs","3) Informations","4) About"};
const byte menu_size=3;

byte lcd_program=0;

/* 
** Some definitions - you should not edit this - except DEBUG if you wish 
*/
#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
#define DBG if(DEBUG)

#define FORMAT_SPIFFS_IF_FAILED true

const char* pver = "PIDKiln v0.2";
const char* pdate = "2019.08.28";
