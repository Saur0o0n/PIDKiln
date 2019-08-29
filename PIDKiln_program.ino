/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//


// Short function to cleanup program def. after failed load
//
byte cleanup_program(byte err){
  Program_size=0;
  Program_desc="";
  for(byte a=0;a<MAX_PRG_LENGTH;a++) Program[a].temp=Program[a].togo=Program[a].dwell=0;
  DBG Serial.printf(" Cleaningup program with error %d\n",err);
  return err;
}


// Load program from disk to memory - validate it
//
byte add_program_line(String& linia){
unsigned int prg[3];
byte a=0,pos=2;
int multi=1;

  // Looking for line dddd:dddd:dddd (temperature:time in minutes:time in minutes) - assume max 1350:9999:9999
  char p_line[15];
  strcpy(p_line,linia.c_str());
  DBG Serial.printf(" Sanitizing line: '%s'\n",p_line);
  a=linia.length(); // going back to front
  prg[2]=0;prg[1]=prg[0]=0;
  while(a--){
    if(p_line[a]=='\0') continue;
    DBG Serial.printf(" %d(%d)\n",(byte)p_line[a],a);
    if(p_line[a]<48 || p_line[a]>58) return 3;  // if this are not numbers or : - exit  
    if(p_line[a]==58){ // separator :
      multi=1;
      pos--;
    }else{
      DBG Serial.printf("pos: %d, multi: %d, prg[%d] is %d, current value:%d\n",pos,multi,pos,prg[pos],p_line[a]-48);
      prg[pos]+=multi*(p_line[a]-48);
      multi*=10;
    }
  }
  if(prg[0]>MAX_Temp) return 4;   // check if we do not exceed max temperature
  Program[Program_size].temp=prg[0];
  Program[Program_size].togo=prg[1];
  Program[Program_size].dwell=prg[2];
  //DBG Serial.printf("\n Program_pos: %d, Temp: %dC Time to: %ds Dwell: %ds\n",Program_size,Program[Program_size].temp,Program[Program_size].togo,Program[Program_size].dwell);
  Program_size++;
  return 0;
}

byte load_program(){
String line;
byte err=0;
char file_path[32];
unsigned int pos=0;
File prg;

  if(!Selected_Program) return cleanup_program(1);
  
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
        if(pos=line.indexOf("#")){
          line=line.substring(0,pos);
          line.trim(); // trim again after removing comment
        }
        
        if(line.length()>15) return cleanup_program(2);  // program line too long
        else if(err=add_program_line(line)) return cleanup_program(err); // line adding failed!!
        
        DBG Serial.printf("San line: '%s'\n",line.c_str());
      }
    }
    DBG Serial.printf("Found description: %s\n",Program_desc.c_str());
    if(!Program_desc.length()) Program_desc="No description";   // if after reading file program still has no decription - add it
    
    return 0;
  }return cleanup_program(1);
  
}
