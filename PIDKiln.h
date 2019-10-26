#include <PID_v1.h>

/*
** Relays and thermocouple defs. and other addons
**
*/

#define EMR_RELAY_PIN 21
#define SSR1_RELAY_PIN 19
//#define SSR2_RELAY_PIN 22   // if you want to use additional SSR for second heater, uncoment this

// MAX31855 variables/defs
#define MAXCS1  27    // for hardware SPI - HSPI (MOSI-13, MISO-12, CLK-14) - 1st device CS-15
//#define MAXCS2  15    // same SPI - 2nd device CS-27 (comment out if no second thermocouple)

// If you have power meter - uncoment this
#define ENERGY_MON_PIN 33       // if you don't use - comment out

#define ALARM_PIN 26        // Pin goes high on abort
uint16_t ALARM_countdown=0; // countdown in seconds to stop alarm

/*
** Temperature, PID and probes variables/definitions
*/
// Temperature & PID variables
double int_temp=20, kiln_temp=20, case_temp=20;
double set_temp, pid_out;
float temp_incr=0;
uint32_t windowStartTime;
#define PID_WINDOW_DIVIDER 1

//Specify the links and initial tuning parameters
PID KilnPID(&kiln_temp, &pid_out, &set_temp, 0, 0, 0, P_ON_E, DIRECT);

/*
** Global value of LCD screen/menu and menu position
**
*/
typedef enum {
  SCR_MAIN_VIEW,      // group of main screens showing running program
  SCR_MENU,           // menu
  SCR_PROGRAM_LIST,   // list of all programs
  SCR_PROGRAM_SHOW,   // showing program content
  SCR_PROGRAM_DELETE, // deleting program
  SCR_PROGRAM_FULL,   // step by step program display
  SCR_QUICK_PROGRAM,  // set manually desire, single program step
  SCR_ABOUT,          // short info screen
  SCR_PREFERENCES,    // show current preferences
  SCR_OTHER           // some other screens like about that are stateless
} LCD_State_enum;

typedef enum { // different main screens
  MAIN_VIEW1,
  MAIN_VIEW2,
  MAIN_VIEW3,
  MAIN_end
} LCD_MAIN_View_enum;

typedef enum { // menu positions
  M_SCR_MAIN_VIEW,
  M_LIST_PROGRAMS,
  M_QUICK_PROGRAM,
  M_INFORMATIONS,
  M_PREFERENCES,
  M_CONNECT_WIFI,
  M_ABOUT,
  M_RESTART,
  M_END
} LCD_SCR_MENU_Item_enum;

LCD_State_enum LCD_State=SCR_MAIN_VIEW;          // global variable to keep track on where we are in LCD screen
LCD_MAIN_View_enum LCD_Main=MAIN_VIEW1;          // main screen has some views - where are we
LCD_SCR_MENU_Item_enum LCD_Menu=M_SCR_MAIN_VIEW; // menu items

const char *Menu_Names[] = {"1) Main view", "2) List programs", "3) Quick program", "4) Information", "5) Preferences", "6) Reconnect WiFi", "7) About", "8) Restart"};

typedef enum { // program menu positions
  P_EXIT,
  P_SHOW,
  P_LOAD,
  P_DELETE,
  P_end
} LCD_PSCR_MENU_Item_enum;

char *Prog_Menu_Names[] = {"Exit","Show","Load","Del."};
const uint8_t Prog_Menu_Size=4;

#define SCREEN_W 128   // LCD screen width and height
#define SCREEN_H 64
#define MAX_CHARS_PL SCREEN_W/3  // char can have min. 3 points on screen

const uint8_t SCR_MENU_LINES=5;   // how many menu lines should be print
const uint8_t SCR_MENU_SPACE=2;   // pixels spaces between lines
const uint8_t SCR_MENU_MIDDLE=3;  // middle of the menu, where choosing will be possible

/*
** Kiln program variables
**
*/
struct PROGRAM {
  uint16_t temp;
  uint16_t togo;
  uint16_t dwell;
};
// maxinum number of program lines (this goes to memory - so be careful)
#define MAX_PRG_LENGTH 40

PROGRAM Program[MAX_PRG_LENGTH];  // We could use here malloc() but...
uint8_t Program_size=0;           // number of actual entries in Program
String Program_desc,Program_name; // First line of the selected program file - it's description

PROGRAM* Program_run;             // running program (made as copy of selected Program)
uint8_t Program_run_size=0;       // number of entries in running program (since elements count from 0 - this value is actually bigger by 1 then numbers of steps)
char *Program_run_desc=NULL,*Program_run_name=NULL;
time_t Program_run_start=0;       // date/time of started program
time_t Program_run_end=0;         // date/time when program ends - during program it's ETA
int Program_run_step=-1;          // at which step are we now... (has to be it - so we can give it -1)
uint16_t Program_start_temp=0;    // temperature on start of the program
uint8_t Program_error=0;          // if program finished with errors - remember number

typedef enum { // program menu positions
  PR_NONE,
  PR_READY,
  PR_RUNNING,
  PR_PAUSED,
  PR_ABORTED,
  PR_ENDED,
  PR_THRESHOLD,
  PR_end
} PROGRAM_RUN_STATE;
PROGRAM_RUN_STATE Program_run_state=PR_NONE; // running program state
const char *Prog_Run_Names[] = {"unknown","Ready","Running","Paused","Aborted","Ended","Waiting"};

/* 
**  Program errors:
*/
typedef enum {
  PR_ERR_FILE_LOAD,       // failed to load file
  PR_ERR_TOO_LONG_LINE,   // program line too long (there is error probably in the line - it should be max. 1111:1111:1111 - so 14 chars, if there where more PIDKiln will throw error without checking why
  PR_ERR_BAD_CHAR,        // not allowed character in program (only allowed characters are numbers and separator ":")
  PR_ERR_TOO_HOT,         // exceeded max temperature defined in MAX_Temp
  PR_ERR_TOO_COLD,        // temperature redout below MIN_Temp
  PR_ERR_MAX31A_INT_ERR,   // failed to read MAX31855 internal temperature on kiln
  PR_ERR_MAX31A_KPROBE,    // failed to read K-probe temperature on kiln
  PR_ERR_MAX31B_INT_ERR,   // failed to read MAX31855 internal temperature on case
  PR_ERR_MAX31B_KPROBE,    // failed to read K-probe temperature on case
  PR_ERR_USER_ABORT,      // user aborted
  PR_ERR_end
} PROGRAM_ERROR_STATE;

/*
** Filesystem definintions
**
*/
#define MAX_FILENAME 30   // directory+name can be max 32 on SPIFFS
#define MAX_PROGNAME 20   //  - cos we already have /programs/ directory...

const char allowed_chars_in_filename[]="abcdefghijklmnoprstuwxyzABCDEFGHIJKLMNOPRSTUWXYZ1234567890._";

struct DIRECTORY {
  char filename[MAX_PROGNAME+1];
  uint16_t filesize=0;
  uint8_t sel=0;
};

DIRECTORY* Programs_DIR=NULL;
uint16_t Programs_DIR_size=0;

DIRECTORY* Logs_DIR=NULL;
uint16_t Logs_DIR_size=0;

/* Directory loading errors:
** 1 - cant open "/programs" directory
** 2 - file name is too long or too short (this should not happened)
** 3 - 
*/

 
/* 
** Spiffs settings
**
*/
#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
const char *PRG_Directory = PRG_DIRECTORY;  // I started to use it so often... so this will take less RAM then define

const char *LOG_Directory = "/logs";

#define FORMAT_SPIFFS_IF_FAILED true

/*
**  Preference definitions
*/
#define PREFS_FILE "/etc/pidkiln.conf"

// Other variables
//
typedef enum { // program menu positions
  PRF_NONE,
  PRF_WIFI_SSID,
  PRF_WIFI_PASS,
  PRF_WIFI_MODE,      // 0 - connect to AP if failed, be AP; 1 - connect only to AP; 2 - be only AP
  PRF_WIFI_RETRY_CNT,
  PRF_WIFI_AP_NAME,
  PRF_WIFI_AP_USERNAME,
  PRF_WIFI_AP_PASS,

  PRF_AUTH_USER,
  PRF_AUTH_PASS,

  PRF_NTPSERVER1,
  PRF_NTPSERVER2,
  PRF_NTPSERVER3,
  PRF_GMT_OFFSET,
  PRF_DAYLIGHT_OFFSET,
  PRF_INIT_DATE,
  PRF_INIT_TIME,
  
  PRF_PID_WINDOW,
  PRF_PID_KP,
  PRF_PID_KI,
  PRF_PID_KD,
  PRF_PID_POE,
  PRF_PID_TEMP_THRESHOLD,

  PRF_LOG_WINDOW,
  PRF_LOG_LIMIT,

  PRF_MIN_TEMP,
  PRF_MAX_TEMP,
  PRF_MAX_HOUS_TEMP,
  PRF_THERMAL_RUN,
  PRF_ALARM_TIMEOUT,
  
  PRF_end
} PREFERENCES;

const char *PrefsName[]={
"None","WiFi_SSID","WiFi_Password","WiFi_Mode","WiFi_Retry_cnt","WiFi_AP_Name","WiFi_AP_Username","WiFi_AP_Pass",
"Auth_Username","Auth_Password",
"NTP_Server1","NTP_Server2","NTP_Server3","GMT_Offset_sec","Daylight_Offset_sec","Initial_Date","Initial_Time",
"PID_Window","PID_Kp","PID_Ki","PID_Kd","PID_POE","PID_Temp_Threshold",
"LOG_Window","LOG_Files_Limit",
"MIN_Temperature","MAX_Temperature","MAX_Housing_Temperature","Thermal_Runaway","Alarm_Timeout",
};

// Preferences types definitions
typedef enum {
 NONE,        // when this value is set - prefs item is off
 UINT8,
 UINT16,
 INT16,
 STRING,
 VFLOAT,
} TYPE;


// Structure for keeping preferences values
struct PrefsStruct {
  TYPE type=NONE;
  union {
    uint8_t uint8;
    uint16_t uint16;
    int16_t int16;
    char *str;
    double vfloat;
  } value;
};

struct PrefsStruct Prefs[PRF_end];

// Pointer to a log file
File CSVFile,LOGFile;

/*
** Other stuff
**
*/
const char *PVer = "PIDKiln v1.0";
const char *PDate = "2019.10.26";

#define DBG if(DEBUG)

/*
** Function defs
**
*/
void load_msg(char msg[MAX_CHARS_PL]);
boolean return_LCD_string(char* msg,char* rest, int mod, uint16_t screen_w=SCREEN_W);
void LCD_Display_program_summary(int dir=0,byte load_prg=0);
void LCD_Display_quick_program(int dir=0,byte pos=0);

uint8_t Cleanup_program(uint8_t err=0);
uint8_t Load_program(char *file=0);
void ABORT_Program(uint8_t error=0);
