/*
** Function for relays and temperature sensors
**
*/
#include <Adafruit_MAX31855.h>

// PID variables
double int_temp=20,kiln_temp=20;

Adafruit_MAX31855 thermocouple(MAXCS);

// Thermocouple temperature readout
//
void Update_temperature(){
uint32_t raw;
double kiln_tmp;

  raw = thermocouple.readRaw();
  int_temp = thermocouple.decodeInternal(raw);
  kiln_tmp = thermocouple.decodeCelsius(raw);
  kiln_temp = thermocouple.linearizeCelcius(int_temp, kiln_tmp);
  
  if (isnan(kiln_temp) || isnan(int_temp)) {
    DBG Serial.println(" !! Something wrong with thermocouple!");
    ABORT_Program(5);
  }else{
    DBG Serial.printf("Temperature readout: Internal = %.2f \t Kiln raw = %.2f \t Kiln final = %.2f\n", int_temp, kiln_tmp, kiln_temp);
  }  
}
