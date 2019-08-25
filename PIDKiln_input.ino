/*
** Pidkiln input (rotary encoder, buttons) subsystem
**
*/

// Other variables
//

// Vars for interrupt function to keep track on encoder
volatile int lastEncoded = 0;
volatile int encoderValue = 0;
volatile unsigned int encoderButton = 0;

// Global value of the encoder position
int lastencoderValueT = 0;
byte menu_pos=0,screen_pos=0;

// Tell compiler it's interrupt function - otherwise it won't work on ESP
void ICACHE_RAM_ATTR handleInterrupt ();


// Interrupt parser for rotary encoder and it's button
//
void handleInterrupt() {
  int MSB = digitalRead(encoder0PinA); //MSB = most significant bit
  int LSB = digitalRead(encoder0PinB); //LSB = least significant bit

  if(digitalRead(encoder0Button)==LOW){
      if(encoderButton<4294967290) encoderButton+=1;
  }else{ // Those two events can be simultanouse - but this is also ok, usualy user does not press and turn
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

void input_loop() {

   if(encoderButton){
      delay(ENCODER_BUTTON_DELAY);
      if(digitalRead(encoder0Button)==LOW) return; // Button is still pressed - skipp, perhaps it's long press
      if(encoderButton>=long_press){
        DBG Serial.println("Button long pressed");
      }else{
        DBG Serial.println("Button pressed");
      }
      encoderButton=false;
   }
   else if(encoderValue!=0){
      delay(ENCODER_ROTATE_DELAY);
      lastencoderValueT+=encoderValue;
      encoderValue=0;
      DBG Serial.print("Encoder: ");
      DBG Serial.println(lastencoderValueT);
  }
}
