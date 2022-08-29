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

   This sensor uses SPI bus to communicate, specials pins are required to interface
   Board:                                    MOSI        MISO        SCLK         SS, don't use for CS   Level
   Uno, Mini, Pro, ATmega168, ATmega328..... 11          12          13           10                     5v
   Mega, Mega2560, ATmega1280, ATmega2560... 51          50          52           53                     5v
   Due, SAM3X8E............................. ICSP4       ICSP1       ICSP3        x                      3.3v
   Leonardo, ProMicro, ATmega32U4........... 16          14          15           x                      5v
   Blue Pill, STM32F103xxxx boards.......... PA17        PA6         PA5          PA4                    3v
   NodeMCU 1.0, WeMos D1 Mini............... GPIO13/D7   GPIO12/D6   GPIO14/D5    GPIO15/D8*             3v/5v
   ESP32.................................... GPIO23/D23  GPIO19/D19  GPIO18/D18   x                      3v

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
#include <MAX31855.h>

int32_t rawData_01  = 0;
int32_t rawData_02  = 0;

/*
  MAX31855(cs)

  cs - chip select
*/

MAX31855 myMAX31855_01(3); //for ESP8266 change to D3 (fails to BOOT/FLASH if pin LOW)
MAX31855 myMAX31855_02(4); //for ESP8266 change to D4 (fails to BOOT/FLASH if pin LOW)

void setup()
{
  Serial.begin(115200);

  /* start MAX31855 */
  myMAX31855_01.begin();
  myMAX31855_02.begin();

  while (myMAX31855_01.getChipID() != MAX31855_ID)
  {
    Serial.println(F("MAX6675_01 error")); //(F()) saves string to flash & keeps dynamic memory free
    delay(5000);
  }
  Serial.println(F("MAX6675_01 OK"));
}

void loop()
{
  while (myMAX31855_01.detectThermocouple() != MAX31855_THERMOCOUPLE_OK)
  {
    switch (myMAX31855_01.detectThermocouple())
    {
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC:
        Serial.println(F("Thermocouple_01 short to VCC"));
        break;

      case MAX31855_THERMOCOUPLE_SHORT_TO_GND:
        Serial.println(F("Thermocouple_01 short to GND"));
        break;

      case MAX31855_THERMOCOUPLE_NOT_CONNECTED:
        Serial.println(F("Thermocouple_01 not connected"));
        break;

      case MAX31855_THERMOCOUPLE_UNKNOWN:
        Serial.println(F("Thermocouple_01 unknown error, check spi cable"));
        break;
    }
    delay(5000);
  }

  rawData_01 = myMAX31855_01.readRawData();
  rawData_02 = myMAX31855_02.readRawData();

  Serial.print(F("Chip_ID_01: "));
  Serial.println(myMAX31855_01.getChipID(rawData_01));

  Serial.print(F("Chip_ID_02: "));
  Serial.println(myMAX31855_02.getChipID(rawData_02));

  Serial.print(F("Cold_Junction_01: "));
  Serial.println(myMAX31855_01.getColdJunctionTemperature(rawData_01));

  Serial.print(F("Cold_Junction_02: "));
  Serial.println(myMAX31855_02.getColdJunctionTemperature(rawData_02));

  Serial.print(F("Thermocouple_01: "));
  Serial.println(myMAX31855_01.getTemperature(rawData_01));

  Serial.print(F("Thermocouple_02: "));
  Serial.println(myMAX31855_02.getTemperature(rawData_02));

  delay(5000);
}
