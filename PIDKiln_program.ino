/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//


// Load program from disk to memory - validate it
//
boolean add_program_line(String& linia){
unsigned int prg[3];
byte a=0,multi=1,pos=2;

  // Looking for line dddd:dddd:dddd (temperature:time in minutes:time in minutes) - assume max 1350:9999:9999
  char p_line[15];
  strcpy(p_line,linia.c_str());
  DBG Serial.printf(" Sanitizing line: '%s'\n",p_line);
  a=linia.length(); // going back to front
  prg[2]=0;prg[1]=prg[0]=0;
  while(a--){
    if(p_line[a]=='\0') continue;
    DBG Serial.printf(" %d(%d)",(byte)p_line[a],a);
    if(p_line[a]<48 || p_line[a]>58) return false;  // if this are not numbers or : - exit  
    if(p_line[a]==58){ // separator :
      multi=1;
      pos--;
    }else{
//      DBG Serial.printf("pos: %d, multi: %d, prg[%d]: %d\n",pos,multi,pos,prg[pos]);
      prg[pos]+=multi*(p_line[a]-48);
      multi*=10;
    }
  }
  Program[Program_size].temp=prg[0];
  Program[Program_size].togo=prg[1];
  Program[Program_size].dwell=prg[2];
  DBG Serial.printf("\n Program_pos: %d, Temp: %dC Time to: %ds Dwell: %ds\n",Program_size,Program[Program_size].temp,Program[Program_size].togo,Program[Program_size].dwell);
  Program_size++;
  return true;
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
