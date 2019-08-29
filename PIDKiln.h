
// Global value of LCD screen/menu and menu position
typedef enum {
  MAIN_VIEW,    // group of main screens showing running program
  MENU,         // menu
  PROGRAM_LIST, // list of all programs
  PROGRAM_SHOW, // showing program content
  OTHER         // some other screens like about that are stateless
} LCD_State_enum;

typedef enum { // different main screens
  MAIN_VIEW1,
  MAIN_VIEW2,
  MAIN_VIEW3,
  MAIN_end
} LCD_MAIN_View_enum;

typedef enum { // menu positions
  M_MAIN_VIEW,
  M_LIST_PROGRAMS,
  M_INFORMATIONS,
  M_ABOUT,
  M_end
} LCD_MENU_Item_enum;

LCD_State_enum LCD_State=MAIN_VIEW;      // global variable to keep track on where we are in LCD screen
LCD_MAIN_View_enum LCD_Main=MAIN_VIEW1;  // main screen has some views - where are we
LCD_MENU_Item_enum LCD_Menu=M_MAIN_VIEW; // menu items

const char *Menu_Names[] = {"1) Main view","2) List programs","3) Information","4) About"};
const byte Menu_Size=3;

byte LCD_Program=0;

/*
** Kiln program variables
*/
String Selected_Program="";   // currently selected program name to open/edit/run
struct PROGRAM {
  unsigned int temp;
  unsigned int togo;
  unsigned int dwell;
};
// maxinum number of program lines (this goes to memory - so be carefull)
#define MAX_PRG_LENGTH 40

PROGRAM Program[MAX_PRG_LENGTH];  // We could use here malloc() and pointers, but since it's not recommended in Arduino and 3*integer is the same as pointers...
byte Program_size=0;  // number of actual entries in Program
String Program_desc;  // First line of the program file - it's description

/* Program errors:
** 1 - failed to load file
** 2 - program line too long (there is error probably in the line - it should be max. 1111:1111:1111 - so 14 chars, if there where more PIDKiln will throw error without checking why
** 3 - not allowed character in program (only allowed characters are numbers and sperator ":")
** 4 - exceeded max temperature defined in MAX_Temp
*/
 
/* 
** Some definitions - you should not edit this
*/
#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
#define DBG if(DEBUG)

#define FORMAT_SPIFFS_IF_FAILED true

const char* PRG_Directory = PRG_DIRECTORY;

const char* pver = "PIDKiln v0.2";
const char* pdate = "2019.08.28";
