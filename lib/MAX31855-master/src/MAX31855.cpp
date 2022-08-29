/***************************************************************************************************/
/*
   This is an Arduino library for 14-bit MAX31855 K-Thermocouple to Digital Converter
   with 12-bit Cold Junction Compensation conneted to hardware 5Mhz SPI with maximum sampling
   rate ~9..10Hz.

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


/**************************************************************************/
/*
    MAX31855()

    Constructor for hardware read only SPI

    NOTE:
    - cs is chip select, set cs low to enable serial interface
*/
/**************************************************************************/
MAX31855::MAX31855(uint8_t cs)
{
  _cs = cs; //cs chip select
}

/**************************************************************************/
/*
    begin()

    Initializes & configures hardware SPI
*/
/**************************************************************************/
void MAX31855::begin(SPIClass *SPI_pointer)
{
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);                  //disables SPI interface for MAX31855, but it will initiate measurement/conversion

  MAXSPI = SPI_pointer;
  MAXSPI->begin();                              //setting hardware SCK, MOSI, SS to output, pull SCK, MOSI low & SS high

  delay(MAX31855_CONVERSION_POWER_UP_TIME);
}

/**************************************************************************/
/*
    detectThermocouple()

    Checks if thermocouple is open, shorted to GND, shorted to VCC

    Return:
    - 0 OK
    - 1 short to VCC
    - 2 short to GND
    - 3 not connected

    NOTE:
    - bit D16 is normally low & goes high if thermocouple is open, shorted to GND or VCC
    - bit D2  is normally low & goes high to indicate a hermocouple short to VCC
    - bit D1  is normally low & goes high to indicate a thermocouple short to GND
    - bit D0  is normally low & goes high to indicate a thermocouple open circuit
*/
/**************************************************************************/
uint8_t MAX31855::detectThermocouple(int32_t rawValue)
{
  if (rawValue == MAX31855_FORCE_READ_DATA) rawValue = readRawData();

  if (bitRead(rawValue, 16) == 1)
  {
    if      (bitRead(rawValue, 2) == 1) return MAX31855_THERMOCOUPLE_SHORT_TO_VCC;
    else if (bitRead(rawValue, 1) == 1) return MAX31855_THERMOCOUPLE_SHORT_TO_GND;
    else if (bitRead(rawValue, 0) == 1) return MAX31855_THERMOCOUPLE_NOT_CONNECTED;
    else                                return MAX31855_THERMOCOUPLE_UNKNOWN;
  }
  return MAX31855_THERMOCOUPLE_OK;
}

/**************************************************************************/
/*
    getChipID()

    Checks chip ID

    NOTE:
    - bit D17, D3 always return zero & can be used as device ID
*/
/**************************************************************************/
uint16_t MAX31855::getChipID(int32_t rawValue)
{
  if (rawValue == MAX31855_FORCE_READ_DATA) rawValue = readRawData();

  if (bitRead(rawValue, 17) == 0 && bitRead(rawValue, 3) == 0) return MAX31855_ID;

  return 0;
}

/**************************************************************************/
/*
    getTemperature()

    Reads Temperature, C

    NOTE:
    - range -200°C..+700°C ±2°C or -270°C..+1372°C ±6°C with 0.25°C
      resolution/increment
    - thermocouple temperature data is 14-bit long
    - bit D31 is the thermocouple temperature sign bit "+" is high & "-" is low,
      if T+ and T- are unconnected it goes low
    - bits D30..D18 contain the converted temperature in the order of MSB to LSB,
      if T+ and T- are unconnected they go high
    - it is strongly recommended to add a 10nF/0.01mF ceramic surface-mount
      capacitor, placed across the T+ and T- pins, to filter noise on the
      thermocouple lines
*/
/**************************************************************************/
float MAX31855::getTemperature(int32_t rawValue)
{
  if (rawValue == MAX31855_FORCE_READ_DATA) rawValue = readRawData();

  if (detectThermocouple(rawValue) != MAX31855_THERMOCOUPLE_OK || getChipID(rawValue) != MAX31855_ID) return MAX31855_ERROR;

  rawValue = rawValue >> 18; //clear D17..D0 bits

  return (float)rawValue * MAX31855_THERMOCOUPLE_RESOLUTION;
}

/**************************************************************************/
/*
    getColdJunctionTemperature()

    Reads Temperature, C

    NOTE:
    - range -40°C..+125° ±3°C with 0.062°C resolution/increment
    - chip internal temperature data is 12-bit long
    - bit D15 is cold-junction temperature sign bit "+" is high & "-" is low
    - bits D14..D4 contain cold-junction temperature in the order of MSB to LSB
*/
/**************************************************************************/
float MAX31855::getColdJunctionTemperature(int32_t rawValue)
{
  if (rawValue == MAX31855_FORCE_READ_DATA) rawValue = readRawData();

  if (getChipID(rawValue) != MAX31855_ID) return MAX31855_ERROR;

  rawValue = rawValue & 0x0000FFFF; //clear D31..D16 bits
  rawValue = rawValue >> 4;         //clear D3...D0  bits

  return (float)rawValue * MAX31855_COLD_JUNCTION_RESOLUTION;
}

/**************************************************************************/
/*
    readRawData()

    Reads raw data from MAX31855 via hardware SPI

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

    - arduino 8-bit AVR maximum SPI master clock speed is mcu speed/2,
      for 5v-16MHz/ProMini speed is 16000000/2=8MHz 
    - arduino ESP8266 maximum SPI master clock speed is 80000000=80MHz
    - arduino STM32 maximum SPI master clock speed is mcu speed/2,
      for STM32F103C8 speed is 72000000/2=36MHz
    - SPI_MODE0 -> data available shortly after the rising edge of SCK
*/
/**************************************************************************/
int32_t MAX31855::readRawData(void)
{
  int32_t rawData = 0;

  digitalWrite(_cs, LOW);                                          //stop  measurement/conversion
  delayMicroseconds(1);                                            //5MHz  is 0.2μsec, do we need it???
  digitalWrite(_cs, HIGH);                                         //start measurement/conversion

  delay(MAX31855_CONVERSION_TIME);

  MAXSPI->beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0)); //up to 5MHz, read MSB first, SPI mode 0, see note

  digitalWrite(_cs, LOW);                                          //set software CS low to enable SPI interface for MAX31855

  for (uint8_t i = 0; i < 2; i++)                                  //read 32-bits via hardware SPI, in order MSB->LSB (D31..D0 bit)
  {
    rawData = (rawData << 16) | MAXSPI->transfer16(0x0000);            //chip has read only SPI & MOSI not connected, so it doesn't metter what to send
  }

  digitalWrite(_cs, HIGH);                                         //disables SPI interface for MAX31855, but it will initiate measurement/conversion

  MAXSPI->endTransaction();                                            //de-asserting hardware CS & free hw SPI for other slaves

  return rawData;
}
