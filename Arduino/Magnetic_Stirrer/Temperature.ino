/*
  ======== Temperature Sensor (LM35) ========
  Connected to A0 (analogue)
  Output: 10 mV per °C
  Arduino Uno R4 ADC: 14-bit (0–16383) with 5V reference by default
  
  Conversion: voltage = (analogRead * 5.0) / 16384.0
              temp_C  = voltage / 0.01
  
  Note: Arduino Uno R4 WiFi has a 14-bit ADC. If using 10-bit mode,
  change 16384 to 1024.
*/

// Smoothing — simple moving average
const int   TEMP_SAMPLES = 10;
float       temp_buffer[TEMP_SAMPLES];
int         temp_index = 0;
bool        temp_buffer_filled = false;

void InitialiseTemperature() {
  pinMode(PIN_LM35, INPUT);
  // Pre-fill buffer
  for (int i = 0; i < TEMP_SAMPLES; i++) {
    temp_buffer[i] = 0.0;
  }
}

// Returns temperature in °C (smoothed)
float GetTemperature() {
  int raw = analogRead(PIN_LM35);
  
  // If 14-bit: divisor = 16384.0
  // If 10-bit: divisor = 1024.0
  float voltage = (raw * 5.0) / 1024.0; // using 10-bit for now
  float temp_c  = voltage / 0.01;       // LM35: 10mV per °C

  // Rolling average
  temp_buffer[temp_index] = temp_c;
  temp_index = (temp_index + 1) % TEMP_SAMPLES;
  if (temp_index == 0) temp_buffer_filled = true;

  int count = temp_buffer_filled ? TEMP_SAMPLES : temp_index;
  float sum = 0;
  for (int i = 0; i < count; i++) {
    sum += temp_buffer[i];
  }
  return sum / count;
}
