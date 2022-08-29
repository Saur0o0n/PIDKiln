/***************************************************************************************************/
/*
   This is an Arduino library for 14-bit MAX31855 K-Thermocouple to Digital Converter
   with 12-bit Cold Junction Compensation conneted to software/bit-bang SPI with maximum
   sampling rate ~9..10Hz.

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
     
   written by : enjoyneering79
   sourse code: https://github.com/enjoyneering/MAX31855

   Board:                                     Level
   Uno, Mini, Pro, ATmega168, ATmega328.....  5v
   Mega, Mega2560, ATmega1280, ATmega2560...  5v
   Due, SAM3X8E.............................  3.3v
   Leonardo, ProMicro, ATmega32U4...........  5v
   Blue Pill, STM32F103xxxx boards..........  3v
   NodeMCU 1.0, WeMos D1 Mini...............  3v/5v*
   ESP32....................................  3v

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

#ifndef MAX31855soft_h
#define MAX31855soft_h

/*
   Unfortunately, you cannot #define something in the sketch & get
   it in the library, because the Arduino toolchain includes library
   files & compiles them in advance, not knowing where it will be used.

   - uncomment to disable interrupts during bit-bang
*/
//#define MAX31855_DISABLE_INTERRUPTS

#define MAX31855_SOFT_SPI //disable upload hw driver spi.h

#include <MAX31855.h>


class MAX31855soft : public MAX31855
{
  public:
   MAX31855soft(uint8_t cs, uint8_t so, uint8_t sck);

   void     begin(void);
   int32_t  readRawData(void);
 
  private:
   uint8_t _so;
   uint8_t _sck;
};

#endif
