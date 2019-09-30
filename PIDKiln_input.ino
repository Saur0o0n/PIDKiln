/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//

// Vars for interrupt function to keep track on encoder
volatile int lastEncoded = 0;
volatile int encoderValue = 0;
volatile unsigned long encoderButton = 0;

// Global value of the encoder position
int lastencoderValueT = 0;
byte menu_pos=0,screen_pos=0;

// Tell compiler it's interrupt function - otherwise it won't work on ESP
void ICACHE_RAM_ATTR handleInterrupt ();


// What to do if button pressed in menu
//
void pressed_menu(){
  switch(LCD_Menu){
    case M_SCR_MAIN_VIEW: LCD_display_main_view(); break;
    case M_LIST_PROGRAMS: LCD_display_programs(); break;
    case M_QUICK_PROGRAM: LCD_Display_quick_program(0,0); break;
    case M_INFORMATIONS: LCD_Display_info(); break;
    case M_PREFERENCES: LCD_Display_prefs(); break;
    case M_CONNECT_WIFI: LCD_Reconect_WiFi(); break;
    case M_ABOUT: LCD_Display_about(); break;
    default: break;
  }
}



// Just redirect pressed button to separate functions
//
void button_Short_Press(){
  DBG Serial.printf(" Short press. Current view %d\n",(int)LCD_State);
  if(LCD_State==SCR_MENU) pressed_menu();
  else if(LCD_State==SCR_MAIN_VIEW && LCD_Main==MAIN_VIEW3) LCD_display_mainv3(0,2);
  else if(LCD_State==SCR_PROGRAM_LIST) LCD_Display_program_summary(0,0);
  else if(LCD_State==SCR_PROGRAM_SHOW) LCD_Display_program_summary(0,2);
  else if(LCD_State==SCR_PROGRAM_DELETE) LCD_Display_program_delete(0,1);
  else if(LCD_State==SCR_PROGRAM_FULL) LCD_Display_program_summary(0,1);
  else if(LCD_State==SCR_QUICK_PROGRAM) LCD_Display_quick_program(0,2);
  else LCD_display_menu();  // if pressed something else - go back to menu
}


// Handle long press on encoder
//
void button_Long_Press(){
  
  if(LCD_State==SCR_MENU){ // we are in menu - switch to main screen
    LCD_State=SCR_MAIN_VIEW;
    LCD_display_main_view();
    return;
  }else if(LCD_State==SCR_PROGRAM_SHOW){ // if we are showing program - go to program list
    LCD_State=SCR_PROGRAM_LIST;
    LCD_display_programs(); // LCD_Program is global
    return;
  }else{ // If we are in MAIN screen or Program list or in unknown area to to -> menu
    LCD_State=SCR_MENU; // switching to menu
    LCD_display_menu();
    return;    
  }
}


// Handle or rotation encoder input
//
void rotate(){

// If we are in MAIN screen view
 if(LCD_State==SCR_MAIN_VIEW){
  if(LCD_Main==MAIN_VIEW3){
    LCD_display_mainv3(encoderValue,1);
    return;
  }
  if(encoderValue<0){
    if(LCD_Main>MAIN_VIEW1) LCD_Main=(LCD_MAIN_View_enum)((int)LCD_Main-1);
    LCD_display_main_view();
    return;
  }else{
    if(LCD_Main<MAIN_end-1) LCD_Main=(LCD_MAIN_View_enum)((int)LCD_Main+1);
    LCD_display_main_view();
    return;
  }
  }else if(LCD_State==SCR_MENU){
    DBG Serial.printf("Rotate, SCR_MENU: Encoder turn: %d, Sizeof menu %d, Menu nr %d, \n",encoderValue, Menu_Size, LCD_Menu);
    if(encoderValue<0){
      if(LCD_Menu>M_SCR_MAIN_VIEW) LCD_Menu=(LCD_SCR_MENU_Item_enum)((int)LCD_Menu-1);
    }else{
      if(LCD_Menu<M_end-1) LCD_Menu=(LCD_SCR_MENU_Item_enum)((int)LCD_Menu+1);
    }
    LCD_display_menu();
    return;
  }else if(LCD_State==SCR_PROGRAM_LIST){
    DBG Serial.printf("Rotate, PROGRAMS: Encoder turn: %d\n",encoderValue);
    rotate_selected_program(encoderValue);
    LCD_display_programs();
  }else if(LCD_State==SCR_PROGRAM_SHOW) LCD_Display_program_summary(encoderValue,1);
  else if(LCD_State==SCR_PROGRAM_DELETE) LCD_Display_program_delete(encoderValue,0);
  else if(LCD_State==SCR_PROGRAM_FULL) LCD_Display_program_full(encoderValue);
  else if(LCD_State==SCR_PREFERENCES) LCD_Display_prefs(encoderValue);
  else if(LCD_State==SCR_QUICK_PROGRAM) LCD_Display_quick_program(encoderValue,1);
}
















// Main input loop task function
//
void Input_Loop(void * parameter) {

  for(;;){
    if(encoderButton){
      vTaskDelay( ENCODER_BUTTON_DELAY / portTICK_PERIOD_MS );
      if(digitalRead(ENCODER0_BUTTON)==LOW) return; // Button is still pressed - skip, perhaps it's a long press
      if(encoderButton+Long_Press>=millis()){ // quick press
        DBG Serial.printf("Button pressed %f seconds\n",(float)(millis()-encoderButton)/1000);
        button_Short_Press();
      }else{  // long press
        DBG Serial.printf("Button long pressed %f seconds\n",(float)(millis()-encoderButton)/1000);
        button_Long_Press();
      }
      encoderButton=0;
    }else if(encoderValue!=0){
      vTaskDelay(ENCODER_ROTATE_DELAY / portTICK_PERIOD_MS);
      rotate(); // encoderValue is global..
      DBG Serial.printf("Encoder rotated %d\n",encoderValue);
      encoderValue=0;
    }
    yield();
  }
}


// Interrupt parser for rotary encoder and it's button
//
void handleInterrupt() {

  if(digitalRead(ENCODER0_BUTTON)==LOW){
      encoderButton=millis();
  }else{ // Those two events can be simultaneous - but this is also ok, usually user does not press and turn
    int MSB = digitalRead(ENCODER0_PINA); //MSB = most significant bit
    int LSB = digitalRead(ENCODER0_PINB); //LSB = least significant bit
    int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
    int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue=1;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue=-1;

    lastEncoded = encoded; //store this value for next time
  }
}


// Setup all input pins and interrupts
//
void Setup_Input() {

  pinMode(ENCODER0_PINA, INPUT); 
  pinMode(ENCODER0_PINB, INPUT);
  pinMode(ENCODER0_BUTTON, INPUT);

  digitalWrite(ENCODER0_PINA, HIGH); //turn pullup resistor on
  digitalWrite(ENCODER0_PINB, HIGH); //turn pullup resistor on
  digitalWrite(ENCODER0_BUTTON, HIGH); //turn pullup resistor on

  attachInterrupt(ENCODER0_PINA, handleInterrupt, CHANGE);
  attachInterrupt(ENCODER0_PINB, handleInterrupt, CHANGE);
  attachInterrupt(ENCODER0_BUTTON, handleInterrupt, FALLING);

  xTaskCreate(
              Input_Loop,       /* Task function. */
              "Input_loop",     /* String with name of task. */
              2048,              /* Stack size in bytes. */
              NULL,             /* Parameter passed as input of the task */
              1,                /* Priority of the task. */
              NULL);            /* Task handle. */
}
