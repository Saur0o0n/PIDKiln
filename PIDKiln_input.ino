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

LCD_State lcd_state=MAIN_VIEW;    // global variable to keep track on where we are in LCD screen
LCD_MAIN_View lcd_main=MAIN_VIEW1;  // main screen has some views - where are we

// Interrupt parser for rotary encoder and it's button
//
void handleInterrupt() {

  if(digitalRead(encoder0Button)==LOW){
      encoderButton=millis();
  }else{ // Those two events can be simultanouse - but this is also ok, usualy user does not press and turn
    int MSB = digitalRead(encoder0PinA); //MSB = most significant bit
    int LSB = digitalRead(encoder0PinB); //LSB = least significant bit
    int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
    int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue=1;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue=-1;

    lastEncoded = encoded; //store this value for next time
  }
}


void setup_input() {

  pinMode(encoder0PinA, INPUT); 
  pinMode(encoder0PinB, INPUT);
  pinMode(encoder0Button, INPUT);

  digitalWrite(encoder0PinA, HIGH); //turn pullup resistor on
  digitalWrite(encoder0PinB, HIGH); //turn pullup resistor on
  digitalWrite(encoder0Button, HIGH); //turn pullup resistor on

  attachInterrupt(encoder0PinA, handleInterrupt, CHANGE);
  attachInterrupt(encoder0PinB, handleInterrupt, CHANGE);
  attachInterrupt(encoder0Button, handleInterrupt, FALLING);
}

void button_pressed(){
  
}

// Handle long press on encoder
//
void button_long_press(){
  if(lcd_state==MAIN_VIEW || lcd_state==PROGRAM_LIST){  // MAIN screen or Program list -> menu
    // switching to menu
    lcd_state=MENU;
    LCD_display_menu();
    return;
  }else  if(lcd_state==MENU){ // we are in menu - switch to main screen
    lcd_state=MAIN_VIEW;
    LCD_display_main(lcd_main);
    return;
  }else if(lcd_state==PROGRAM_SHOW){ // if we are showing program - go to program list
    lcd_state=PROGRAM_LIST;
    LCD_display_programs(); // lcd_program is global
    return;
  }
}

// Handle or rotation encoder input
//
void rotate(){

// If we are in MAIN screen view
 if(lcd_state==MAIN_VIEW){
  if(encoderValue<0){
    if(lcd_main>MAIN_VIEW1) lcd_main=(LCD_MAIN_View)((int)lcd_main-1);
    LCD_display_main(lcd_main);
    return;
  }else{
    if(lcd_main<MAIN_end-1) lcd_main=(LCD_MAIN_View)((int)lcd_main+1);
    LCD_display_main(lcd_main);
    return;
  }
 }else if(lcd_state==MENU){
   DBG Serial.printf("Rotate, MENU: Encoder turn: %d, Sizeof menu %d, Menu nr %d, \n",encoderValue, menu_size,lcd_menu);
   if(encoderValue<0){
     if(lcd_menu>0) lcd_menu--;
   }else{
     if(lcd_menu<menu_size) lcd_menu++;
   }
   LCD_display_menu();
   return;
 }else if(lcd_state==PROGRAM_LIST){
  
 }else if(lcd_state==PROGRAM_SHOW){
  
 }
}

// Main input loop
//
void input_loop() {

   if(encoderButton){
      delay(ENCODER_BUTTON_DELAY);
      if(digitalRead(encoder0Button)==LOW) return; // Button is still pressed - skipp, perhaps it's a long press
      if(encoderButton+long_press>=millis()){ // quick press
        DBG Serial.printf("Button pressed %f seconds\n",(float)(millis()-encoderButton)/1000);
        button_pressed();
      }else{  // long press
        DBG Serial.printf("Button long pressed %f seconds\n",(float)(millis()-encoderButton)/1000);
        button_long_press();
      }
      encoderButton=0;
   }
   else if(encoderValue!=0){
      delay(ENCODER_ROTATE_DELAY);
      rotate(); // encoderValue is global..
      DBG Serial.printf("Encoder rotated %d\n",encoderValue);
      encoderValue=0;
  }
}
