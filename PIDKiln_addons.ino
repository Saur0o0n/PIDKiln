/*
** Function for relays (SSR, EMR) and temperature sensors
**
*/
#include <Adafruit_MAX31855.h>

// Initialize SPI and MAX31855
SPIClass *ESP32_SPI = new SPIClass(HSPI);
Adafruit_MAX31855 thermocouple(MAXCS);

boolean SSR_On; // just to narrow down state changes.. I don't know if this is needed

// Simple functions to enable/disable SSR - for clarity, everything is separate
//
void Enable_SSR(){
  if(!SSR_On){
//    DBG Serial.println("[ADDONS] Enable SSR");
    digitalWrite(SSR_RELAY_PIN, HIGH);
    SSR_On=true;
  }
}

void Disable_SSR(){
  if(SSR_On){
//    DBG Serial.println("[ADDONS] Disable SSR");
    digitalWrite(SSR_RELAY_PIN, LOW);
    SSR_On=false;
  }
}

void Enable_EMR(){
  //digitalWrite(EMR_RELAY_PIN, HIGH);
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
    DBG Serial.println("[ADDONS] !! Something wrong with MAX31855! Internal readout failed");
    ABORT_Program(PR_ERR_MAX31_INT_ERR);
    return;
  }
  int_temp = (int_temp+kiln_tmp1)/2;
  
  kiln_tmp1 = thermocouple.decodeCelsius(raw);
  kiln_tmp2 = thermocouple.linearizeCelcius(int_temp, kiln_tmp1);
  
  if (isnan(kiln_tmp1) || isnan(kiln_tmp2)) {
    DBG Serial.println("[ADDONS] !! Something wrong with thermocouple! External readout failed");
    ABORT_Program(PR_ERR_MAX31_KPROBE);
    return;
  }
  kiln_temp=(kiln_temp*0.9+kiln_tmp2*0.1);    // We try to make bigger hysteresis

  //DBG Serial.printf("Temperature readout: Internal = %.1f \t Kiln raw = %.1f \t Kiln final = %.1f\n", int_temp, kiln_tmp1, kiln_temp); 
}


void Setup_Addons(){
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR_RELAY_PIN, OUTPUT);

  SSR_On=false;
  thermocouple.begin(ESP32_SPI);
}
