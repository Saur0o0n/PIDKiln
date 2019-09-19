/*
** Function for relays (SSR, EMR) and temperature sensors
**
*/
#include <Adafruit_MAX31855.h>

// Initialize SPI and MAX31855
SPIClass *ESP32_SPI = new SPIClass(HSPI);
Adafruit_MAX31855 thermocouple(MAXCS);

// Simple functions to enable/disable SSR - for clarity, everything is separate
//
void Enable_SSR(){
  digitalWrite(SSR_RELAY_PIN, HIGH);
}
void Disable_SSR(){
  digitalWrite(SSR_RELAY_PIN, LOW);
}
void Enable_EMR(){
  digitalWrite(EMR_RELAY_PIN, HIGH);
}
void Disable_EMR(){
  digitalWrite(EMR_RELAY_PIN, LOW);
}



// Thermocouple temperature readout
//
void Update_Temperature(){
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
  if(kiln_temp<22){
    Enable_EMR();
    Enable_SSR();
    DBG Serial.println("Enabled EMR");
  }
  if(kiln_temp>25){
    Disable_SSR();
    Disable_EMR();
    DBG Serial.println("Disabled EMR");
  }
  DBG Serial.printf("Temperature readout: Internal = %.1f \t Kiln raw = %.1f \t Kiln final = %.1f\n", int_temp, kiln_tmp1, kiln_temp); 
}


void Setup_Addons(){
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR_RELAY_PIN, OUTPUT);

  thermocouple.begin(ESP32_SPI);
}
