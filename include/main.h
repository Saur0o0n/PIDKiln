#include <Syslog.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <FS.h>   // Include the SPIFFS library
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <soc/rtc_wdt.h>
#include <esp_task_wdt.h>
#include <MAX31855.h>
#include <PID_v1.h>
#include <soc/efuse_reg.h>
#include <Esp.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <U8g2lib.h>


/* 
** Some definitions - usually you should not edit this, but you may want to
*/
#define ENCODER_BUTTON_DELAY 150  // 150ms between button press readout
#define ENCODER_ROTATE_DELAY 120  // 120ms between rotate readout




// If you have power meter - uncoment this
//#define ENERGY_MON_PIN 33       // if you don't use - comment out

#define ALARM_PIN 26        // Pin goes high on abort

#define PID_WINDOW_DIVIDER 1



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

/* 
**  Program errors:
*/
typedef enum {
  PR_ERR_FILE_LOAD,       // failed to load file
  PR_ERR_TOO_LONG_LINE,   // program line too long (there is error probably in the line - it should be max. 1111:1111:1111 - so 14 chars, if there where more PIDKiln will throw error without checking why
  PR_ERR_BAD_CHAR,        // not allowed character in program (only allowed characters are numbers and separator ":")
  PR_ERR_TOO_HOT,         // exceeded max temperature defined in MAX_Temp
  PR_ERR_TOO_COLD,        // temperature redout below MIN_Temp
  PR_ERR_MAX31A_NC,       // MAX31855 A not connected
  PR_ERR_MAX31A_INT_ERR,  // failed to read MAX31855 internal temperature on kiln
  PR_ERR_MAX31A_KPROBE,   // failed to read K-probe temperature on kiln
  PR_ERR_MAX31B_NC,       // MAX31855 B not connected
  PR_ERR_MAX31B_INT_ERR,  // failed to read MAX31855 internal temperature on case
  PR_ERR_MAX31B_KPROBE,   // failed to read K-probe temperature on case
  PR_ERR_USER_ABORT,      // user aborted
  PR_ERR_end
} PROGRAM_ERROR_STATE;

/*
** Filesystem definintions
**
*/
#define MAX_FILENAME 30   // directory+name can be max 32 on SPIFFS
#define MAX_PROGNAME 20   //  - cos we already have /programs/ directory...

struct DIRECTORY {
  char filename[MAX_PROGNAME+1];
  uint16_t filesize=0;
  uint8_t sel=0;
};


 
/* 
** Spiffs settings
**
*/
#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x

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

  PRF_HTTP_JS_LOCAL,

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
  PRF_ERROR_GRACE_COUNT,

  PRF_DBG_SERIAL,
  PRF_DBG_SYSLOG,
  PRF_SYSLOG_SRV,
  PRF_SYSLOG_PORT,

  PRF_end
} PREFERENCES;


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



// If defined debug - do debug, otherwise comment out all debug lines
#define DBG if(DEBUG)



#define JS_JQUERY "https://ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js"
#define JS_CHART "https://cdn.jsdelivr.net/npm/chart.js@2.9.3/dist/Chart.bundle.min.js"
#define JS_CHART_DS "https://cdn.jsdelivr.net/npm/chartjs-plugin-datasource"


/*
** Function defs
**
*/

uint8_t Cleanup_program(uint8_t err=0);
uint8_t Load_program(char *file=0);
void ABORT_Program(uint8_t error=0);

boolean delete_file(File &newFile);
boolean check_valid_chars(byte a);
boolean valid_filename(char *file);



void Enable_SSR();
void Disable_SSR();
void Enable_EMR();
void Disable_EMR();
void print_bits(uint32_t raw);
void Update_TemperatureA();
void T_UpdateTemperature(void *params);
void Update_TemperatureB();
void Read_Energy_INPUT();
void Power_Loop(void * parameter);
void STOP_Alarm();
void START_Alarm();
void SetupMAX31856(void);
void Setup_Addons();

String Preferences_parser(const String& var);
String Debug_ESP32(const String& var);

void Generate_INDEX();
void Generate_LOGS_INDEX();
String Chart_parser(const String& var);
String About_parser(const String& var);

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void POST_Handle_Delete(AsyncWebServerRequest *request);
void GET_Handle_Delete(AsyncWebServerRequest *request);
void GET_Handle_Load(AsyncWebServerRequest *request);
void handlePrefs(AsyncWebServerRequest *request);
void handleIndexPost(AsyncWebServerRequest *request);
String handleVars(const String& var);

void out(const char *s);
void do_screenshot(AsyncWebServerRequest *request);

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
bool _webAuth(AsyncWebServerRequest *request);

void SETUP_WebServer(void);
void STOP_WebServer();

void pressed_menu();
void button_Short_Press();
void button_Long_Press();
void Rotate();
void Input_Loop(void * parameter);
void handleInterrupt();
void Setup_Input();

void Init_log_file();
void Add_log_line();
void Close_log_file();
void Clean_LOGS();
uint8_t Load_LOGS_Dir();

void dbgLog(uint16_t pri, const char *fmt, ...);

void initSysLog();
void initSerial();

void printLocalTime();
void Setup_start_date();
void Return_Current_IP(IPAddress &lip);
void Disable_WiFi();
boolean Start_WiFi_AP();
boolean Start_WiFi_CLIENT();
boolean Setup_WiFi();
boolean Restart_WiFi();

boolean Change_prefs_value(String item, String value);
void Save_prefs();
void Load_prefs();
void Prefs_updated_hook();
void Setup_prefs(void);

void IRAM_ATTR onTimer();
byte add_program_line(String& linia);
uint8_t Load_program(char *file);
uint8_t Load_programs_dir();
void Update_program_step(uint8_t sstep, uint16_t stemp, uint16_t stime, uint16_t sdwell);
void Initialize_program_to_run();
void Load_program_to_run();

int Find_selected_program();
void rotate_selected_program(int dir);
byte Cleanup_program(byte err);
boolean Erase_program_file();
void END_Program();
void ABORT_Program(uint8_t error);
void PAUSE_Program();
void RESUME_Program();
void Program_recalculate_ETA(boolean dwell=false);
void Program_calculate_steps(boolean prg_start=false);
void START_Program();
void SAFETY_Check();
void Program_Loop(void * parameter);
void Program_Setup();


