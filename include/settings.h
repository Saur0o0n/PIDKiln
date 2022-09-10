/* 
** Static, editable parameters. Some of them, can be replaces with PIDKiln preferences.
** Please set them up according to your hardware configuration before compiling and uploading.
*/

// If you have Wrover with PSRAM
//#define MALLOC ps_malloc
//#define REALLOC ps_realloc

// if you have Wroom without it
#define MALLOC malloc
#define REALLOC realloc

/*
** Debug setting
** Define true if you want to see debug messages in UART
*/
#define DEBUG true
//#define DEBUG false


/*
** Input settings
** If you use buttons instead of encoder the scheme is following: 
** Press on pin A is for clockwise turn
** Press on pin B is for counterclocwise turn
*/
#define ENCODER0_PINA    34
#define ENCODER0_PINB    35
#define ENCODER0_BUTTON  32

/*
**Uncomment this if you want buttons instead of encoder
*/
//#define USE_BUTTONS


/*
** Relays and thermocouple defs. and other addons
**
*/

#define EMR_RELAY_PIN 18
#define SSR1_RELAY_PIN 19
//#define SSR2_RELAY_PIN 22   // if you want to use additional SSR for second heater, uncoment this

// MAX31855 variables/defs
#define MAXCS1  27    // for hardware SPI - HSPI (MOSI-13, MISO-12, CLK-14) - 1st device CS-27
//#define MAXCS2  15    // same SPI - 2nd device CS-15 (comment out if no second thermocouple)

