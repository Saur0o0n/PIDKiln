
/* 
** Global value of LCD screen/menu and menu position 
*/
typedef enum {
  MAIN_VIEW,    // group of main screens showing running program
  MENU,         // menu
  PROGRAM_LIST, // list of all programs
  PROGRAM_SHOW, // showing program content
  PROGRAM_DELETE,  // deleting program
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

typedef enum { // program menu positions
  P_EXIT,
  P_SHOW,
  P_LOAD,
  P_DELETE,
  P_end
} LCD_PMENU_Item_enum;

const char *Prog_Menu_Names[] = {"Exit","Show","Load","Del."};
const uint8_t Prog_Menu_Size=4;

uint8_t LCD_Program=0;

#define SCREEN_W 128   // LCD screen width and height
#define SCREEN_H 64
#define MAX_CHARS_PL SCREEN_W/3  // char can have min. 3 points on screen

static uint8_t MENU_LINES=5;   // how many menu lines should be print
static uint8_t MENU_SPACE=2;   // pixels spaces between lines
static uint8_t MENU_MIDDLE=3;  // middle of the menu, where choosing will be possible

/*
** Kiln program variables
*/
struct PROGRAM {
  uint16_t temp;
  uint16_t togo;
  uint16_t dwell;
};
// maxinum number of program lines (this goes to memory - so be careful)
#define MAX_PRG_LENGTH 40

PROGRAM Program[MAX_PRG_LENGTH];  // We could use here malloc() and pointers...

uint8_t Program_size=0;  // number of actual entries in Program
String Program_desc;  // First line of the program file - it's description

/* Program errors:
** 1 - failed to load file
** 2 - program line too long (there is error probably in the line - it should be max. 1111:1111:1111 - so 14 chars, if there where more PIDKiln will throw error without checking why
** 3 - not allowed character in program (only allowed characters are numbers and sperator ":")
** 4 - exceeded max temperature defined in MAX_Temp
*/

/*
** Filesystem definintions
*/

#define MAX_FILENAME 30   // directory+name can be max 32 on SPIFFS
#define MAX_PROGNAME 20   //  - cos we already have /programs/ directory...

struct DIRECTORY {
  char filename[MAX_PROGNAME+1];
  uint16_t filesize=0;
  uint8_t sel=0;
};

DIRECTORY* Programs_DIR;
uint16_t Programs_DIR_size=0;
/* Directory loading errors:
** 1 - cant open "/programs" directory
** 2 - file name is too long or too short (this should not happened)
** 3 - 
*/

 
/* 
** Some definitions - you should not edit this
*/
#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
#define DBG if(DEBUG)

#define FORMAT_SPIFFS_IF_FAILED true

const char* PRG_Directory = PRG_DIRECTORY;  // I started to use it so often... so this will take less RAM then define

const char* PVer = "PIDKiln v0.3";
const char* PDate = "2019.09.01";

/*
** Function defs
*/

void load_msg(char msg[MAX_CHARS_PL]);
boolean return_LCD_string(char* msg,char* rest,int mod=0);
void LCD_Display_program_summary(int dir=0,byte load_prg=0);

uint8_t cleanup_program(uint8_t err=0);
uint8_t load_program(char *file=0);
