#include <Wire.h>
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"
#include <U8g2lib.h> //For OLED
#include <SPI.h>
#include <RotaryEncoder.h>

enum {MENU1, IDLE, SETTINGS, STIRRING}; 
enum {EXPLICIT, IMPLICIT};
int overall_state = MENU1;
int control_mode_state = IMPLICIT;

struct Inputs {
  int knobDir; //1 clockwise 0 idle -1 anticlockwise
  int buttonState; //maybe 1 for pressed, 2 for held, 0 for idle
};

Inputs inputs;
void setup() {

  Serial.begin(115200);
  InitialiseInputs();
  //setupLoad();
  //IntialiseTempSensor();
  //InitialiseDCMotor();
  //InitialiseStepperMotor();
  InitialiseOLED();

}

void loop() {
  inputs = GetInputs();
  switch (overall_state) {
    case MENU1:
      overall_state = UI(MENU1, inputs);
  }
}

Inputs GetInputs() {
  Inputs values;
  values.buttonState = GetButtonState();
  values.knobDir = GetRotaryDir();
  return values;
}

