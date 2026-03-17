
//pins D2 D3 for rotary encoder
//pin for button set below
RotaryEncoder encoder(2, 3, RotaryEncoder::LatchMode::FOUR3);

const int btnPin = 4;
unsigned long btnPressTime = 0;  // Stopwatch start time
bool isHolding = false;         
const int holdDuration = 1000;   // 1000ms = 1 second

void InitialiseInputs() {
  pinMode(btnPin, INPUT_PULLUP);
}
int GetRotaryDir() {
  encoder.tick(); 
  int direction = (int)encoder.getDirection(); // Tells you 1 (CW) or -1 (CCW)
  return direction;
}

int GetButtonState() {
  int btnState = digitalRead(btnPin);
  static int lastBtnState = HIGH;
  int state = 0;

  // button pressed
  if (btnState == LOW && lastBtnState == HIGH) {
    btnPressTime = millis();  // start time
    isHolding = false;        // reset hold
  }

  // long press
  if (btnState == LOW) {
  
    if (millis() - btnPressTime >= holdDuration && !isHolding) {
      //Serial.println("LONG PRESS DETECTED! ");
      state = 2;
      isHolding = true; 
    }
  }

  // button released
  if (btnState == HIGH && lastBtnState == LOW) {
    // if released early
    if (millis() - btnPressTime < holdDuration) {
      //Serial.println("SHORT CLICK DETECTED! (e.g., Select)");
      state = 1;
    }
    btnPressTime = 0; // Reset stopwatch
  }

  lastBtnState = btnState;
  return state;
}
  
