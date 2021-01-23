#include <XInput.h>
#include <Wire.h>
#include "i2c.h"
#include "i2c_MMA8451.h"
MMA8451 mma8451;

// Rumble
int i_pin_rumble = 16;
uint8_t rumbleValue;
static float xyz_g[3];
int x = 0;
int y = 0;

// Nudge
int i_debounce = 180;
int i_nudge_ms = 50;
int i_delta = 0;
int reset = 1;
unsigned long ul_t1;
unsigned long ul_millis;
bool b_nudge_started = false;
bool b_nudge_ended = false;
unsigned long ul_t_nudge_started;
int i_max_nudge = 0;
int i_contrained;
int i_level;
int i_increment;

bool b_left = false;
bool b_right = false;
bool b_bottom = false;
const int ci_max_nudge = 65;

// Plunger
int i_pin_pot_linear = A2;
int i_plunger;
unsigned long ul_t_plunger_deactivated;
bool b_plunger_active = false;
bool b_nudge_active = true;

// Flips
const int DIN_PIN_RIGHT_FLIP = 7;
const int DOUT_RIGHT_RELAY = 6;
const int DIN_PIN_LEFT_FLIP = 5;
const int DOUT_LEFT_RELAY = 4;

int i_right_flip;
int i_left_flip;
int i_prev_right_flip;
int i_prev_left_flip;
bool b_reset = false;
int i_debounce_solenoid = 370;
unsigned long ul_t_relay_activated;
bool b_relay_activated = false;

// Buttons
const int DIN_PIN_LEFT_BUTTON = 14;
const int DIN_PIN_RIGHT_BUTTON = 15;
int i_left_button;
int i_right_button;

unsigned long ul_t_rumble_activated;
bool b_rumble_active;
void setup() {
  XInput.setReceiveCallback(rumbleCallback);
  XInput.begin();
  mma8451.initialize();
  pinMode(DIN_PIN_RIGHT_FLIP, INPUT_PULLUP);
  pinMode(DIN_PIN_LEFT_BUTTON, INPUT_PULLUP);
  pinMode(DIN_PIN_RIGHT_BUTTON, INPUT_PULLUP);
  
  pinMode(DOUT_RIGHT_RELAY, OUTPUT);

  pinMode(i_pin_rumble, OUTPUT);
  
  pinMode(DIN_PIN_LEFT_FLIP, INPUT_PULLUP);
  pinMode(DOUT_LEFT_RELAY, OUTPUT);

  XInput.setTriggerRange(0, 1023);
  XInput.setJoystickRange(0, 1023);
  ul_t_rumble_activated = millis();

  // Fun init 
  digitalWrite(i_pin_rumble, 1);
  delay(50);
  digitalWrite(i_pin_rumble, 0);
  delay(50);
  digitalWrite(i_pin_rumble, 1);
  delay(100);
  digitalWrite(i_pin_rumble, 0);
  delay(100);
  digitalWrite(i_pin_rumble, 1);
  delay(150);
  digitalWrite(i_pin_rumble, 0);
  
  digitalWrite(DOUT_RIGHT_RELAY, 1);
  delay(100);
  digitalWrite(DOUT_RIGHT_RELAY, 0);
  delay(200);
  digitalWrite(DOUT_LEFT_RELAY, 1);
  delay(100);
  digitalWrite(DOUT_LEFT_RELAY, 0);
  delay(100);
  digitalWrite(DOUT_LEFT_RELAY, 1);
  delay(100);
  digitalWrite(DOUT_LEFT_RELAY, 0);
  Serial.begin(9600); 
}

void rumbleCallback(uint8_t packetType) {
  // If we have an LED packet (0x01), do nothing
  if (packetType == (uint8_t) XInputReceiveType::LEDs) {
    return;
  }else if (packetType == (uint8_t) XInputReceiveType::Rumble) {
   rumbleValue = XInput.getRumbleLeft() | XInput.getRumbleRight();
   digitalWrite(i_pin_rumble, rumbleValue);
   if (rumbleValue > 0){
     ul_t_rumble_activated = millis();
   }
  }
}

void loop() {
 // Plunger
 i_plunger = map(constrain(analogRead(i_pin_pot_linear), 80, 880), 80, 880, 0, 512);
 XInput.setJoystick(JOY_RIGHT, 0, 512-i_plunger);

 // Prevent nudge false positive while plunger is bouncing
 if ((i_plunger > 100) && !b_plunger_active){
  b_plunger_active = true;
  b_nudge_active = false;
 }

 // Store time when pluger is released
 if ((i_plunger < 100) && b_plunger_active){
  ul_t_plunger_deactivated = millis();
  b_plunger_active = false;
 }

 // Reactivation nudge afer 100 ms last plunger action
 if (!b_plunger_active){
   if ((millis() - ul_t_plunger_deactivated) > 100){
    b_nudge_active = true;
   }
 }

 // Right flip
 i_right_flip = !digitalRead(DIN_PIN_RIGHT_FLIP);
 XInput.setButton(TRIGGER_RIGHT, i_right_flip);
 if (!i_prev_right_flip && i_right_flip){
   digitalWrite(DOUT_RIGHT_RELAY, 1);
   ul_t_relay_activated = millis();
   b_relay_activated = true;
 }

 // Left flip
 i_left_flip = !digitalRead(DIN_PIN_LEFT_FLIP);
 XInput.setButton(TRIGGER_LEFT, i_left_flip);
 if (!i_prev_left_flip && i_left_flip){
   digitalWrite(DOUT_LEFT_RELAY, 1);
   ul_t_relay_activated = millis();
   b_relay_activated = true;
 }

 // Deactivate nudge during coil activation to avoid false positive nudging
 if (b_relay_activated){
   if (millis() - ul_t_relay_activated < 50){
    b_nudge_active = false;
   }else{
    b_relay_activated = false;
    b_nudge_active = true;
   }

   // Deactivate relay after 10 ms, avoid stick of coils 
   if (millis() - ul_t_relay_activated > 10){
     digitalWrite(DOUT_RIGHT_RELAY, 0);
     digitalWrite(DOUT_LEFT_RELAY, 0);
   }
 }

 if (millis() - ul_t_rumble_activated < 150){
  b_rumble_active = true;
 }else{
  b_rumble_active = false;
 }

 // Left button
 i_left_button = !digitalRead(DIN_PIN_LEFT_BUTTON);
 XInput.setButton(BUTTON_A, i_left_button);

 // Right button
 i_right_button = !digitalRead(DIN_PIN_RIGHT_BUTTON);
 XInput.setButton(BUTTON_B, i_right_button);

 // Store state of pins to activate coils on rising edge
 i_prev_right_flip = i_right_flip;
 i_prev_left_flip = i_left_flip;
 
 // Nudge
 if (b_nudge_active && !b_rumble_active){
   nudge();
 }
}

void nudge(){
  mma8451.getMeasurement(xyz_g);
  x = xyz_g[0]*100;
  y = xyz_g[1]*100;

  XInput.setJoystick(JOY_LEFT,512+map(constrain(x,-ci_max_nudge,ci_max_nudge), 
                                      -ci_max_nudge, 
                                      ci_max_nudge, 
                                      -512,
                                      512),
                              512+map(constrain(y,-ci_max_nudge,ci_max_nudge),
                                      -ci_max_nudge, 
                                      ci_max_nudge, 
                                      -512,
                                      512));                            
  XInput.setJoystick(JOY_LEFT,512, 512);
}
