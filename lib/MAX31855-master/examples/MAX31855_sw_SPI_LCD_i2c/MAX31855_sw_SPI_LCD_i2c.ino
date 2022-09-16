/***************************************************************************************************/
/* 
   Example for 12-bit MAX6675 K-Thermocouple to Digital Converter with Cold Junction Compensation

   written by : enjoyneering79
   sourse code: https://github.com/enjoyneering/MAX6675

   - MAX31855 maximum power supply voltage is 3.6v
   - K-type thermocouples have an absolute accuracy of around ±2°C..±6°C.
   - Measurement tempereture range -200°C..+700°C ±2°C or -270°C..+1372°C ±6°C
     with 0.25°C resolution/increment.
   - Cold junction compensation range -40°C..+125° ±3°C with 0.062°C resolution/increment.
     Optimal performance of cold junction compensation happends when the thermocouple cold junction
     & the MAX31855 are at the same temperature. Avoid placing heat-generating devices or components
     near the converter because this may produce an errors.
   - It is strongly recommended to add a 10nF/0.01mF ceramic surface-mount capacitor, placed across
     the T+ and T- pins, to filter noise on the thermocouple lines.

   This lcd uses I2C bus to communicate, specials pins are required to interface
   Board:                                    SDA                    SCL                    Level
   Uno, Mini, Pro, ATmega168, ATmega328..... A4                     A5                     5v
   Mega2560................................. 20                     21                     5v
   Due, SAM3X8E............................. 20                     21                     3.3v
   Leonardo, Micro, ATmega32U4.............. 2                      3                      5v
   Digistump, Trinket, ATtiny85............. 0/physical pin no.5    2/physical pin no.7    5v
   Blue Pill, STM32F103xxxx boards.......... PB7                    PB6                    3.3v/5v
   ESP8266 ESP-01........................... GPIO0/D5               GPIO2/D3               3.3v/5v*
   NodeMCU 1.0, WeMos D1 Mini............... GPIO4/D2               GPIO5/D1               3.3v/5v*
   ESP32.................................... GPIO21/D21             GPIO22/D22             3.3v

                                             *most boards has 10-12kOhm pullup-up resistor on GPIO2/D4 & GPIO0/D3
                                              for flash & boot

   Frameworks & Libraries:
   ATtiny  Core          - https://github.com/SpenceKonde/ATTinyCore
   ESP32   Core          - https://github.com/espressif/arduino-esp32
   ESP8266 Core          - https://github.com/esp8266/Arduino
   STM32   Core          - https://github.com/stm32duino/Arduino_Core_STM32
                         - https://github.com/rogerclarkmelbourne/Arduino_STM32

   GNU GPL license, all text above must be included in any redistribution,
   see link for details  - https://www.gnu.org/licenses/licenses.html
*/
/***************************************************************************************************/
#include <Wire.h>               //for esp8266 use bug free i2c driver https://github.com/enjoyneering/ESP8266-I2C-Driver
#include <MAX31855soft.h>
#include <LiquidCrystal_I2C.h>  //https://github.com/enjoyneering/LiquidCrystal_I2C

#define LCD_ROWS           4    //qnt. of lcd rows
#define LCD_COLUMNS        20   //qnt. of lcd columns

#define LCD_DEGREE_SYMBOL  0xDF //degree symbol from lcd ROM, see p.9 of GDM2004D datasheet
#define LCD_SPACE_SYMBOL   0x20 //space  symbol from lcd ROM, see p.9 of GDM2004D datasheet

#define MAX_TEMPERATURE    45   //max temp, in °C

const uint8_t iconTemperature[8] PROGMEM = {0x04, 0x0E, 0x0E, 0x0E, 0x0E, 0x1F, 0x1F, 0x0E}; //PROGMEM saves variable to flash & keeps dynamic memory free

float temperature = 0;

/*
MAX31855soft(cs, so, sck)

cs  - chip select
so  - serial data output
sck - serial clock input
*/

MAX31855soft      myMAX31855(3, 4, 7); //for ESP8266 change to D3 (fails to BOOT/FLASH if pin LOW), D4 (fails to BOOT/FLASH if pin LOW), D7
LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);


void setup()
{
  Serial.begin(115200);

  /* LCD connection check */  
  while (lcd.begin(LCD_COLUMNS, LCD_ROWS, LCD_5x8DOTS) != true) //20x4 display, 5x8 pixels size
  {
    Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
    delay(5000);
  }

  lcd.print(F("PCF8574 is OK")); //(F()) saves string to flash & keeps dynamic memory free
  delay(1000);

  lcd.clear();

  /* start MAX31855 */
  myMAX31855.begin();

  while (myMAX31855.getChipID() != MAX31855_ID)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("MAX31855 error"));
    delay(5000);
  }

  lcd.clear();

  while (myMAX31855.detectThermocouple() != MAX31855_THERMOCOUPLE_OK)
  {
    lcd.setCursor(0, 0);
    switch (myMAX31855.detectThermocouple())
    {
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC:
        lcd.print(F("short to Vcc "));
        break;

      case MAX31855_THERMOCOUPLE_SHORT_TO_GND:
        lcd.print(F("short to GND "));
        break;

      case MAX31855_THERMOCOUPLE_NOT_CONNECTED:
        lcd.print(F("not connected"));
        break;

      case MAX31855_THERMOCOUPLE_UNKNOWN:
        lcd.print(F("unknown error"));                            //check spi cable
        break;
    }
    delay(5000);
  }

  lcd.clear();

  lcd.print(F("MAX31855 OK"));
  delay(2000);

  lcd.clear();

  /* load custom symbol to CGRAM */
  lcd.createChar(0, iconTemperature);

  /* prints static text */
  lcd.setCursor(0, 0);
  lcd.write(0);                                                   //print temperature icon
  lcd.setCursor(0, 1);
  lcd.write(0);
  
}

void loop()
{
  temperature = myMAX31855.getTemperature();

  lcd.setCursor(2, 0);
  if (temperature != MAX31855_ERROR) lcd.print(temperature, 1);
  else                               lcd.print(F("xx"));          //thermocouple short to Vcc, or to GND, or not connected
  lcd.write(LCD_DEGREE_SYMBOL);
  lcd.print(F("C"));
  lcd.write(LCD_SPACE_SYMBOL);

  lcd.printHorizontalGraph('T', 2, temperature, MAX_TEMPERATURE); //name of the bar, 3-rd row, current value, max. value

  temperature = myMAX31855.getColdJunctionTemperature();

  lcd.setCursor(2, 1);
  if (temperature != MAX31855_ERROR) lcd.print(temperature, 1);
  else                               lcd.print(F("xx"));          //error on spi bus or not MAX31855 sensor
  lcd.write(LCD_DEGREE_SYMBOL);
  lcd.print(F("C"));
  lcd.write(LCD_SPACE_SYMBOL);

  lcd.printHorizontalGraph('T', 3, temperature, 30);              //name of the bar, 4-rd row, current value, max. value

  delay(1000);
}
