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
double kiln_tmp1,kiln_tmp2;

  raw = thermocouple.readRaw();
  kiln_tmp1 = thermocouple.decodeInternal(raw); 
  if (isnan(kiln_tmp1)) {
    DBG Serial.println(" !! Something wrong with MAX31855! Internal readout failed");
    ABORT_Program(5);
    return;
  }
  int_temp = (int_temp+kiln_tmp1)/2;
  
  kiln_tmp1 = thermocouple.decodeCelsius(raw);
  kiln_tmp2 = thermocouple.linearizeCelcius(int_temp, kiln_tmp1);
  
  if (isnan(kiln_tmp1) || isnan(kiln_tmp2)) {
    DBG Serial.println(" !! Something wrong with thermocouple! External readout failed");
    ABORT_Program(6);
    return;
  }
  kiln_temp=(kiln_temp+kiln_tmp2)/2;
  DBG Serial.printf("Temperature readout: Internal = %.1f \t Kiln raw = %.1f \t Kiln final = %.1f\n", int_temp, kiln_tmp1, kiln_temp); 
}
