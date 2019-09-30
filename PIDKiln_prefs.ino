/*
** Preferences stuff
**
*/


// Find and change item in preferences
//
boolean Change_prefs_value(String item, String value){
  for(uint16_t a=0; a<PRF_end; a++){
    if(item.equalsIgnoreCase(String(PrefsName[a]))){  // we have found an matching prefs value
      if(Prefs[a].type==STRING){
        if(Prefs[a].value.str) free(Prefs[a].value.str);
        Prefs[a].value.str=strdup(value.c_str());
        DBG Serial.printf("  -> For %s saved STRING item value:%s type:%d\n",PrefsName[a],Prefs[a].value.str,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==UINT8){
        Prefs[a].value.uint8=(uint8_t)value.toInt();
        DBG Serial.printf("  -> For %s saved UINT8 item value:%d type:%d\n",PrefsName[a],Prefs[a].value.uint8,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==UINT16){
        Prefs[a].value.uint16=(uint16_t)value.toInt();
        DBG Serial.printf("  -> For %s saved UINT16 item value:%d type:%d\n",PrefsName[a],Prefs[a].value.uint16,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==INT16){
        Prefs[a].value.int16=(uint16_t)value.toInt();
        DBG Serial.printf("  -> For %s saved INT16 item value:%d type:%d\n",PrefsName[a],Prefs[a].value.int16,(int)Prefs[a].type);
        return true;
      }else if(Prefs[a].type==VFLOAT){
        Prefs[a].value.vfloat=(double)value.toDouble();
        DBG Serial.printf("  -> For %s saved VFLOAT item value:%f type:%d\n",PrefsName[a],Prefs[a].value.vfloat,(int)Prefs[a].type);
        return true;
      }
    }
  }
  return false;
}


// Save prefs to file
//
void Save_prefs(){
File prf;

  DBG Serial.println("[PREFS] Writing prefs to file");
  if(prf=SPIFFS.open(PREFS_FILE,"w")){
    for(uint16_t a=1; a<PRF_end; a++){
      if(Prefs[a].type==STRING){
        prf.printf("%s = %s\n",PrefsName[a],Prefs[a].value.str);
      }else if(Prefs[a].type==UINT8){
        prf.printf("%s = %d\n",PrefsName[a],Prefs[a].value.uint8);
      }else if(Prefs[a].type==UINT16){
        prf.printf("%s = %d\n",PrefsName[a],Prefs[a].value.uint16);
      }else if(Prefs[a].type==INT16){
        prf.printf("%s = %d\n",PrefsName[a],Prefs[a].value.int16);
      }
    }
    prf.flush();
    prf.close();
  }
}


// Read prefs from file
//
void Load_prefs(){
File prf;
String line,item,value;
int pos=0;

  DBG Serial.println("[PREFS] Loading prefs from file");
  if(prf=SPIFFS.open(PREFS_FILE,"r"))
    while(prf.available()){
      line=prf.readStringUntil('\n');
      line.trim();
      if(!line.length()) continue;         // empty line - skip it
      if(line.startsWith("#")) continue;   // skip every comment line
      if(pos=line.indexOf("#")){
        line=line.substring(0,pos);
        line.trim(); // trim again after removing comment
      }
      // Now we have a clean line - parse it
      if((pos=line.indexOf("="))>2){          // we need to have = sign - if not, ignore
        item=line.substring(0,pos-1);
        item.trim();
        value=line.substring(pos+1);
        value.trim();
        //DBG Serial.printf("[PREFS] Preference (=@%d) item: '%s' = '%s'\n",pos,item.c_str(),value.c_str());
        
        if(item.length()>2 && value.length()>0) Change_prefs_value(item,value);
      }
    }

    // For debuging only
    DBG Serial.println("[PREFS] -=-=-= PREFS DISPLAY =-=-=-");
    for(uint16_t a=0; a<PRF_end; a++){
      if(Prefs[a].type==STRING) DBG Serial.printf(" %d) '%s' = '%s'\t%d\n",a,PrefsName[a],Prefs[a].value.str,(int)Prefs[a].type);
      if(Prefs[a].type==UINT8) DBG Serial.printf(" %d) '%s' = '%d'\t%d\n",a,PrefsName[a],Prefs[a].value.uint8,(int)Prefs[a].type);
      if(Prefs[a].type==UINT16) DBG Serial.printf(" %d) '%s' = '%d'\t%d\n",a,PrefsName[a],Prefs[a].value.uint16,(int)Prefs[a].type);
      if(Prefs[a].type==INT16) DBG Serial.printf(" %d) '%s' = '%d'\t%d\n",a,PrefsName[a],Prefs[a].value.int16,(int)Prefs[a].type);
    }
}


// This function is called whenever user changes preferences - perhaps we want to do something after that?
//
void Prefs_updated_hook(){

  // We have running program - update PID parameters
  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    KilnPID.SetTunings(Prefs[PRF_PID_KP].value.vfloat,Prefs[PRF_PID_KI].value.vfloat,Prefs[PRF_PID_KD].value.vfloat); // set actual PID parameters
  }
}

/* 
** Setup preferences for PIDKiln
**
*/
void Setup_Prefs(void){
char tmp[30];

  // Fill the preferences with default values - if there is such a need
  DBG Serial.println("[PREFS] Preference initialization");
  for(uint16_t a=1; a<PRF_end; a++)
    switch(a){
      case PRF_WIFI_SSID:
        Prefs[PRF_WIFI_SSID].type=STRING;
        Prefs[PRF_WIFI_SSID].value.str=strdup("");
        break;
      case PRF_WIFI_PASS:
        Prefs[PRF_WIFI_PASS].type=STRING;
        Prefs[PRF_WIFI_PASS].value.str=strdup("");
        break;
      case PRF_WIFI_MODE:
        Prefs[PRF_WIFI_MODE].type=UINT8;
        Prefs[PRF_WIFI_MODE].value.uint8=1;
        break;
      case PRF_WIFI_RETRY_CNT:
        Prefs[PRF_WIFI_RETRY_CNT].type=UINT8;
        Prefs[PRF_WIFI_RETRY_CNT].value.uint8=6;
        break;

      case PRF_NTPSERVER1:
        Prefs[PRF_NTPSERVER1].type=STRING;
        Prefs[PRF_NTPSERVER1].value.str=strdup("pool.ntp.org");
        break;
      case PRF_NTPSERVER2:
        Prefs[PRF_NTPSERVER2].type=STRING;
        Prefs[PRF_NTPSERVER2].value.str=strdup("");
        break;
      case PRF_NTPSERVER3:
        Prefs[PRF_NTPSERVER3].type=STRING;
        Prefs[PRF_NTPSERVER3].value.str=strdup("");
        break;
      case PRF_GMT_OFFSET:
        Prefs[PRF_GMT_OFFSET].type=INT16;
        Prefs[PRF_GMT_OFFSET].value.int16=0;
        break;
      case PRF_DAYLIGHT_OFFSET:
        Prefs[PRF_DAYLIGHT_OFFSET].type=INT16;
        Prefs[PRF_DAYLIGHT_OFFSET].value.int16=0;
        break;
      case PRF_INIT_DATE:
        Prefs[PRF_INIT_DATE].type=STRING;
        Prefs[PRF_INIT_DATE].value.str=strdup("2019-09-08");
        break;
      case PRF_INIT_TIME:
        Prefs[PRF_INIT_TIME].type=STRING;
        Prefs[PRF_INIT_TIME].value.str=strdup("12:00:00");
        break;

      case PRF_MIN_TEMP:
        Prefs[PRF_MIN_TEMP].type=UINT8;
        Prefs[PRF_MIN_TEMP].value.uint8=10;
        break;
      case PRF_MAX_TEMP:
        Prefs[PRF_MAX_TEMP].type=UINT16;
        Prefs[PRF_MAX_TEMP].value.uint16=1350;
        break;

      case PRF_PID_WINDOW:  // how often recaluclate SSR on/off - 5second window default
        Prefs[PRF_PID_WINDOW].type=UINT16;
        Prefs[PRF_PID_WINDOW].value.uint16=5000;
        break;
      case PRF_PID_KP:
        Prefs[PRF_PID_KP].type=VFLOAT;
        Prefs[PRF_PID_KP].value.vfloat=10;
        break;
      case PRF_PID_KI:
        Prefs[PRF_PID_KI].type=VFLOAT;
        Prefs[PRF_PID_KI].value.vfloat=0.2;
        break;
      case PRF_PID_KD:
        Prefs[PRF_PID_KD].type=VFLOAT;
        Prefs[PRF_PID_KD].value.vfloat=0.1;
        break;
      case PRF_PID_POE:   // it's acctualy boolean - but I did not want to create additional type - if we use  Proportional on Error (true) or Proportional on Measurement (false)
        Prefs[PRF_PID_POE].type=UINT8;
        Prefs[PRF_PID_POE].value.uint8=0;
        break;
      case PRF_PID_TEMP_THRESHOLD:  // allowed difference in temperature between set and current when controler will go in dwell mode
        Prefs[PRF_PID_TEMP_THRESHOLD].type=INT16;
        Prefs[PRF_PID_TEMP_THRESHOLD].value.int16=-1;
        break;

      case PRF_LOG_WINDOW:
        Prefs[PRF_LOG_WINDOW].type=UINT16;
        Prefs[PRF_LOG_WINDOW].value.uint16=30;
        break;
        
      default:
        break;
    }
  Load_prefs();
}
