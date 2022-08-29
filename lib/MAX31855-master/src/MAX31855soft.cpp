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

#include <MAX31855soft.h>


/**************************************************************************/
/*
    MAX31855()

    Constructor for software/bit-bang read only SPI

    NOTE:
    cs  - chip select, set CS low to enable the serial interface
    so  - serial data output
    sck - serial clock input
*/
/**************************************************************************/
MAX31855soft::MAX31855soft(uint8_t cs, uint8_t so, uint8_t sck) : MAX31855(cs)
{
  _so  = so;  //sw miso
  _sck = sck; //sw sclk
}

/**************************************************************************/
/*
    begin()

    Initializes & configures soft/bit-bang SPI
*/
/**************************************************************************/
void MAX31855soft::begin(void)
{
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);                  //disables SPI interface for MAX31855, but it will initiate measurement/conversion

  pinMode(_so, INPUT);

  pinMode(_sck, OUTPUT);
  digitalWrite(_sck, LOW);

  delay(MAX31855_CONVERSION_POWER_UP_TIME);
}

/**************************************************************************/
/*
    readRawData()

    Reads raw data from MAX31855 via software/bit-bang SPI

    NOTE:
    - read of the cold-junction compensated thermocouple temperature requires
      14 clock cycles
    - read of the cold-junction compensated thermocouple temperature & reference
      junction temperatures requires 32 clock cycles
    - forcing CS low immediately stops any conversion process, force CS high
      to initiate a new measurement process
    - set CS low to enable the serial interface & force to output the first bit on the SO pin,
      apply 14/32 clock signals at SCK to read the results at SO on the falling edge of the SCK
    - bit D31 is the thermocouple temperature sign bit "+" is high & "-" is low,
      if T+ & T- pins are unconnected it goes low
    - bits D30..D18 contain the converted temperature in the order of MSB to LSB,
      if T+ & T- pins are unconnected they go high
    - bit D17 is low to provide a device ID for the MAX31855
    - bit D16 is normally low & goes high if thermocouple is open, shorted to GND or VCC
    - bit D15 is cold-junction temperature sign bit "+" is high & "-" is low
    - bits D14..D4 contain cold-junction temperature in the order of MSB to LSB
    - bit D3 is is low to provide a device ID for the MAX31855
    - bit D2 is normally low & goes high to indicate a hermocouple short to VCC
    - bit D1 is normally low & goes high to indicate a thermocouple short to GND
    - bit D0 is normally low & goes high to indicate a thermocouple open circuit

    - max SPI clock speed for MAX31855 is only 5MHz
    - SPI_MODE0 -> data available shortly after the rising edge of SCK
*/
/**************************************************************************/
int32_t MAX31855soft::readRawData(void)
{
  int32_t rawData = 0;

  digitalWrite(_cs, LOW);                        //stop  measurement/conversion
  delayMicroseconds(1);                          //5MHz  is 0.2μsec, do we need it???
  digitalWrite(_cs, HIGH);                       //start measurement/conversion

  delay(MAX31855_CONVERSION_TIME);

  digitalWrite(_cs, LOW);                        //set CS low to enable SPI interface for MAX31855

  #ifdef MAX31855_DISABLE_INTERRUPTS
  noInterrupts();                                //disable all interrupts for critical operations below
  #endif

  /* emulate SPI_MODE0 */
  for (int8_t i = 32; i > 0; i--)                //read 32-bits via software SPI, in order MSB->LSB (D31..D0 bit)
  {
    digitalWrite(_sck, HIGH);                    //data available shortly after rising edge of SCK
    rawData = (rawData << 1) | digitalRead(_so);
    digitalWrite(_sck, LOW);                     //data is clocked out on falling edge of SCK
  }

  #ifdef MAX31855_DISABLE_INTERRUPTS
  interrupts();                                  //re-enable all interrupts
  #endif

  digitalWrite(_cs, HIGH);                       //disables SPI interface for MAX31855, but it will initiate measurement/conversion

  return rawData;
}
