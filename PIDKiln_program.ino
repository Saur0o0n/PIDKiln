/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//


// Load program from disk to memory - validate it
//
boolean add_program_line(String& dupa){
unsigned int p_temp=0,p_time=0,p_dwell=0;
char p_line[16];


  // Looking for linex line dddd:dddd:dddd (temperature:time in minutes:time in minutes) - assume max 9999:9999:9999
  DBG Serial.printf(" Sanitizing line: '%s'\n",dupa.c_str());
  for(byte a=0;a<sizeof(p_line);a++){
    
  }
  Program_size++;  
}

boolean load_program(){
String line;
char file_path[32];
unsigned int pos=0;
File prg;

  if(!Selected_Program) return false;
  
  DBG Serial.printf("Load program name %s\n",Selected_Program.c_str());
  sprintf(file_path,"%s/%s",PRG_Directory,Selected_Program.c_str());
  if(prg = SPIFFS.open(file_path,"r")){
    Program_desc="";  // erase current program
    Program_size=0;
    while(prg.available()){
      line=prg.readStringUntil('\n');
      line.trim();
      if(!line.length()) continue; // empty line - skip it
      
      DBG Serial.printf("Raw line: '%s'\n",line.c_str());
      if(line.startsWith("#")){ // skip every comment line
        DBG Serial.println("  comment");
        if(!Program_desc.length()) Program_desc=line.substring(2); // If it's the first line with comment - copy it without trailing #
      }else{
        // Sanitize every line - if it's have a comment - strip ip, then check if this are only numbers and ":" - otherwise fail
        if(pos=line.indexOf("#")) line=line.substring(0,pos);
        
        if(line.length()>15 || !add_program_line(line)) return false; // line adding failed!!
        
        DBG Serial.printf("San line: '%s'\n",line.c_str());
      }
    }
    DBG Serial.printf("Found description: %s\n",Program_desc.c_str());
    if(!Program_desc.length()) Program_desc="No description";   // if after reading file program still has no decription - add it
    
    return true;
  }return false;
  
}
