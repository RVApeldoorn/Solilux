#include <Servo.h>
#include <ezButton.h>
#include <Wire.h>
#include "RTClib.h"

// Stepper motor pins
#define dirPin_lux 2 
#define stepPin_lux 3
#define dirPin_zon 4 
#define stepPin_zon 5
#define StepOn 12

// Other hardware pins
#define Mic_pin A10
#define Servo1_pin 10
#define Servo2_pin 9
#define Buzzer 5
#define LDR_pin A14
#define Relay_pin 34

// LED pins
const int LedPins[] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};

// Buttons (ezButton library)
const int ButtonPins[] = {41, 38, 42, 45, 43, 37, 40, 39, 36};

ezButton buttons[9] = {
  ezButton(ButtonPins[0]), ezButton(ButtonPins[1]), ezButton(ButtonPins[2]),
  ezButton(ButtonPins[3]), ezButton(ButtonPins[4]), ezButton(ButtonPins[5]),
  ezButton(ButtonPins[6]), ezButton(ButtonPins[7]), ezButton(ButtonPins[8])
};

// Structs for timers and LDRs
struct dataNode {
    int dag;
    int uur;
    int minuut;
    String settings;
    int on;
};

struct dataNode2 {
    int ll;
    String settings;
    int on;
};

// Arrays for position/preset/status
int positions[] = {100, 0, 50, 75, 25, 100, 0};
int Stepper_Lux[] = {0, 3968, 0, dirPin_lux, stepPin_lux};
int Servo_25[] = {0, 30, 60, 90, 120};
int Step_25[] = {0, 25, 50, 75, 100};
int ldr_status[4] = {0};
String Characters[] = {"!", "@", "$", "%", "^", "&", "*", ";", ":", "/"};
dataNode Timers[10] = {};
dataNode2 LDRs[4] = {};
String Mic_functions[2];
String Ldr_functions[2];
String Presets[8];

// Variables
int InputStep = 0, TakeSteps = 0, Servo1_pos = 0, Servo2_pos = 0;
int Mic_reading = 0, Clapcount = 0, stepcount = 0, Relay_state = false;
bool Sspeed = false;
bool Buzz_on = true, Mic_on = true, Ldr_on = true, Tim_on = true;
bool Timers_set = false, LDR_set = false;

String Incoming_string, Current_code;
char Incoming_char;
int ser2val = 0, current_Parsed = 0;
int LDRValue = 0, prev_LDRValue = 0, startLDR = 0;
int last_ldrtime = -1;

// Hardware instances
Servo Servo1, Servo2;
RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);

  // Declare stepper and buzzer pins as output
  pinMode(stepPin_lux, OUTPUT);
  pinMode(dirPin_lux, OUTPUT);
  pinMode(stepPin_zon, OUTPUT);
  pinMode(dirPin_zon, OUTPUT);
  pinMode(Buzzer, OUTPUT);

  // Declare LED pins as output
  for (int i = 0; i < 12; i++) {
      pinMode(LedPins[i], OUTPUT);
  }

  // Initialize relay and stepper power
  pinMode(Relaypin, OUTPUT);
  digitalWrite(Relaypin, HIGH);
  pinMode(StepOn, OUTPUT);
  digitalWrite(StepOn, HIGH);

  digitalWrite(LedPins[1], HIGH);
  digitalWrite(LedPins[3], HIGH);
  digitalWrite(LedPins[5], HIGH);
  digitalWrite(LedPins[10], HIGH);
  
  // Attach & initialize servos
  Servo1.attach(Servo1_pin);
  Servo2.attach(Servo2_pin);
  Servo1.write(0);
  Servo2.write(0);
  delay(2000);
  Servo2.detach();
  
  // Initialize timers, presets, and LDR settings
  for (int i = 0; i < 10; i++) {
      Timers[i].dag = Timers[i].uur = Timers[i].minuut = -1;
      Timers[i].settings = "";
      Timers[i].on = 0;
      if (i < 8) Presets[i] = "#xu-t-z-";
      if (i < 4) LDRs[i] = {-1, "", 0};
  }
  
  // Initialize Mic and LDR functions
  for (int i = 0; i < 2; i++) {
      Mic_functions[i] = Ldr_functions[i] = "#xu-t-z-";
  }

  // RTC setup
  if (!rtc.begin()) {
      Serial.println("Couldn't find RTC. Program stopped.");
  }

  if (rtc.lostPower()) {
      Serial.println("RTC lost power");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// Calibrate height blinds via physical buttons
void Calibrate_a() {
  Buzz(4);
  while (true) {
    // Blink calibrating LED
    digitalWrite(LedPins[1], rtc.now().second() % 2 == 0 ? HIGH : LOW);

    // Button 8, move motor clockwise
    if (digitalRead(ButtonPins[7]) == LOW) {
      digitalWrite(dirPin_lux, LOW);  // Set direction
      digitalWrite(StepOn, LOW);      // Enable higher step (more quiet)

      for (int i = 0; i < 50; i++) {
        digitalWrite(stepPin_lux, HIGH);
        delayMicroseconds(8);
        digitalWrite(stepPin_lux, LOW);
        delayMicroseconds(8);
      }
    }

    // Button 9, move motor counterclockwise
    if (digitalRead(ButtonPins[8]) == LOW) {
      digitalWrite(dirPin_lux, HIGH);  // Set opposite direction
      digitalWrite(StepOn, LOW);       // Enable higher step (more quiet)

      for (int i = 0; i < 50; i++) {
        digitalWrite(stepPin_lux, HIGH);
        delayMicroseconds(8);
        digitalWrite(stepPin_lux, LOW);
        delayMicroseconds(8);
      }
    }

    digitalWrite(StepOn, HIGH); // Disable stepping

    // Button 7, exit calibration
    buttons[6].loop();
    if (buttons[6].isPressed()) {
      Buzz(4);
      break;
    }
  }

  // Reset position after calibration
  Stepper_Lux[0] = 0; // New LOW position
  digitalWrite(LedPins[1], LOW); // Turn off calibrating LED
  digitalWrite(LedPins[3], HIGH); // LOW position indicator
}

// Calibration height blinds via bluetooth
void Calibrate_b() {
  Buzz(4);
  Incoming_string = "";
  Current_code = "";

  while (true) {
    // Blink calibrating LED
    digitalWrite(LedPins[1], rtc.now().second() % 2 == 0 ? HIGH : LOW);

    // Check for incoming serial data and handle the code
    if (Serial.available() > 0) {
      Incoming_char = Serial.read();
      if (Incoming_char == ',') {
        if (Incoming_string.length() > 3 && Incoming_string[0] == '#') {
          Current_code = Incoming_string;
          Incoming_string = "";
        }
      } else {
        Incoming_string += Incoming_char;
      }
    }

    // Calibration logic
    if (Current_code == "#kb0" || Current_code == "#kb1") {
      // Determine direction based on the received code
      int direction = (Current_code == "#kb0") ? LOW : HIGH;
      
      // Update the stepper direction
      digitalWrite(dirPin_lux, direction);
      digitalWrite(StepOn, LOW); // Enable higher step (more quiet)

      // Perform a step
      digitalWrite(stepPin_lux, HIGH);
      delayMicroseconds(8);
      digitalWrite(stepPin_lux, LOW);
      delayMicroseconds(8);
    }

    // Finish calibration on receiving "#ka1"
    if (Current_code == "#ka1") {
      Stepper_Lux[0] = 0; // New LOW position
      
      Buzz(4);
      digitalWrite(LedPins[1], LOW); // Turn off calibrating LED
      digitalWrite(LedPins[3], HIGH); // LOW position indicator
      return;
    }

    // Set Stepper to high once done
    digitalWrite(StepOn, HIGH);
  }
}

// Parse and execute received bluetooth signals
void Parse_and_execute(String code) {
  if (code.length() < 3) return;

  Buzz(0);

  // Light Controls
  if (code[1] == 'l') LightSwitch();
  if (code[2] == 'l') LightSwitch_2();
  if (code[2] == 'a') LightSwitch_2_1();
  if (code[2] == 'o') LightSwitch_2_2();

  // Luxaflex Tilt
  current_Parsed = Parse_data(code, 1);
  if (current_Parsed != -1 && current_Parsed / 1.5 != Servo2_pos) {
    Servo2.attach(Servo2_pin);
    int targetPos = current_Parsed / 1.5;

    if (Sspeed) {
      Servo2.write(targetPos);
      delay(2000);
    } else {
        int step = (targetPos > Servo2_pos) ? 1 : -1;
        for (; Servo2_pos != targetPos; Servo2_pos += step) {
          Servo2.write(Servo2_pos);
          delay(50);
        }
    }

    Servo2_pos = targetPos;
    Servo2.detach();

    // LED Indicators for OPEN and CLOSED positions
    digitalWrite(LedPins[4], current_Parsed == 270);
    digitalWrite(LedPins[5], current_Parsed == 0);
    if (current_Parsed != 0 && current_Parsed != 270) {
      digitalWrite(LedPins[4], LOW);
      digitalWrite(LedPins[5], LOW);
    }
  }

  // Luxaflex Height
  current_Parsed = Parse_data(code, 0);
  if (current_Parsed != -1) {
    Stepper_All(current_Parsed);
  }
}

// Extracts data from Bluetooth received string
int Parse_data(String data, int select) {
  struct ParseParams {
    char startChar;
    char endChar;
  };

  const ParseParams params[] = {
    {'u', 't'},  // 0: Blinds Height
    {'t', 'z'},  // 1: Blinds tilt
    {'\0', '#'}, // 3: Index in Timer/Preset/ClapTrigger/LDR array
    {'!', '@'},  // 4 RTC - Day
    {'*', '$'},  // 5 RTC - Hour
    {'$', '/'},  // 6 RTC - Minute
    {'@', '*'},  // 7 RTC - Year
    {'c', '!'},  // 8 RTC - Month
    {'/', '\0'}, // 9 RTC - Seconds
    {'t', '.'},  // 10: Timer - Day
    {'.', '.'},  // 11: Timer - Hour
    {'.', '#'},  // 12: Timer - Minute
    {'m', '\0'}, // 13: Timer - Select by position in fixed list
    {'n', '#'}   // 14: LRD threshold
  };

  if (select < 0 || select >= 15) return -1; // Invalid select value

  char start = params[select].startChar;
  char end = params[select].endChar;

  // Remove everything before the start character
  if (start != '\0') {
    int startIndex = data.indexOf(start) + 1;
    if (startIndex == 0) return -1; // Start character not found
    data.remove(0, startIndex);
  }

  // Select 12: skip two dots (distinction from hour)
  if (select == 12) {
    int dotIndex = data.indexOf('.');
    if (dotIndex == -1) return -1; // First dot not found
    data.remove(0, dotIndex + 1);
  }

  // Remove everything after the end character
  if (end != '\0') {
    int endIndex = data.indexOf(end);
    if (endIndex != -1) data.remove(endIndex);
  }
  
  return (data == "-") ? -1 : data.toInt();
}

// Detects 1 or 2 claps and triggers corresponding actions
void ClapTrigger() {
  Clapcount++;
  delay(500);

  for (int i = 0; i < 500; i++) {
    if (analogRead(Mic_pin) > 600) {
      Clapcount++;
    }
    delay(1);
  }

  int actionIndex = (Clapcount > 1) ? 1 : 0;
  Buzz(actionIndex);
  Parse_and_execute(Mic_functions[actionIndex]);

  Clapcount = 0;
}

// Toggles the speed of blinds tilt
void BlindsSpeed() {
  Sspeed = !Sspeed;
  
  // Update LED indicators
  digitalWrite(LedPins[10], !Sspeed);
  digitalWrite(LedPins[11], Sspeed);
}

// Controls a servo to pull the string for turning the light switch on and off
void LightSwitch() {
  for (Servo1_pos = 0; Servo1_pos <= 120; Servo1_pos += 1) { 
    Servo1.write(Servo1_pos);             
    delay(10);                 
  }
  for (Servo1_pos = 120; Servo1_pos >= 0; Servo1_pos -= 1) { 
    Servo1.write(Servo1_pos);              
    delay(5);                      
  }
}

// Toggle light, regardless of current state
void LightSwitch_2() {
  digitalWrite(Relay_pin, !Relay_state); 
  Relay_state = !Relay_state;
}

// Turn light on
void LightSwitch_2_1() {
  if (Relay_state) return;
  digitalWrite(Relay_pin, LOW);
  Relay_state = true;
}

// Turn relay off
void LightSwitch_2_2() {
  if (!Relay_state) return;
  digitalWrite(Relay_pin, HIGH);
  Relay_state = false;
}

// Adjusts stepper position in 25% increments based on direction
int Nextv_stepper(bool dir) {
  // Calculate the current percentage
  int value = (Stepper_Lux[0] * 100) / Stepper_Lux[1];

  if (dir) { // Moving up
    for (int step : Step_25) {
      if (step > value) return step;
    }
    return 100; // Max value
  }

  for (int i = 4; i >= 0; i--) { // Moving down
    if (Step_25[i] < value) return Step_25[i];
  }
  return 0; // Min value
}

// Adjusts servo position in 25% increments based on direction
int Nextv_servo(bool dir) {
  if (dir) { // Opening
    for (int step : Servo_25) {  
      if (step > Servo2_pos) {
        return step;
      }     
    }
    return 180; // Max position
  }

  // Closing
  for (int i = 4; i >= 0; i--) { 
    if (Servo_25[i] < Servo2_pos) {
      return Servo_25[i];
    }
  }
  return 0; // Min position
}

// Controls the stepper motor movement based on the target position (0-100%)
void Stepper_All(int pos) {
  // Validate input range
  if (pos < 0 || pos > 100) return;

  // Update LED indicators
  digitalWrite(LedPins[2], pos == 100);
  digitalWrite(LedPins[3], pos == 0);
  
  // Convert percentage to step count
  InputStep = (pos / 100.0) * Stepper_Lux[1];

  // If already at the desired position, do nothing
  if (InputStep == Stepper_Lux[0]) return;

  // Determine direction and number of steps to move
  if (InputStep>Stepper_Lux[0]) {
    digitalWrite(Stepper_Lux[3], LOW);
    TakeSteps = InputStep - Stepper_Lux[0];
  } else {
    digitalWrite(Stepper_Lux[3], HIGH);
    TakeSteps = Stepper_Lux[0] - InputStep;
  }

  // Move the stepper motor
  digitalWrite(StepOn, LOW);
  for (int j = 0; j < 1000; j++) {
    for (int i = 0; i < TakeSteps; i++) {
      digitalWrite(Stepper_Lux[4], HIGH);
      delayMicroseconds(15);
      digitalWrite(Stepper_Lux[4], LOW);
      delayMicroseconds(15);
    }
  }

  // Update current step position
  Stepper_Lux[0] = InputStep;
  digitalWrite(StepOn, HIGH);
}

// Controls buzzer
void Buzz(int select) {
  if (Buzz_on || select == 5) {
    tone(Buzzer, 1000, 50);

    // Repeat tones if selected
    int repeatCount = 1;
    if (select == 2) repeatCount = 2;
    else if (select == 3) repeatCount = 3;
    else if (select == 4) {
      tone(Buzzer, 500, 400);
      return;
    }

    for (int i = 0; i < repeatCount; i++) {
      delay(100);
      tone(Buzzer, 1000, 50);
      if (i < repeatCount - 1) delay(100);
    }
  }
}

// Repositions the servo for a full turn to fix stuck blade :)
void Reposition() {
  Servo2.attach(Servo2_pin);
  if (Sspeed) {
    int targetPos = (Servo2_pos >= 90) ? 0 : 180;
    Servo2.write(targetPos);
    delay(2000);
    Servo2.write(Servo2_pos);
    delay(2000);
  } else {
    int Direction = (Servo2_pos >= 90) ? -1 : 1;
    int targetPos = (Servo2_pos >= 90) ? 0 : 180;

    for (int i = Servo2_pos; i != targetPos; i += Direction) {
      Servo2.write(i);
      delay(delayTime);
    }

    for (int i = targetPos; i != Servo2_pos; i -= Direction) {
      Servo2.write(i);
      delay(delayTime);
    }
  }

  Servo2.detach();
}

// Extracts all settings from Bluetooth received string
String Parse_string(String data, int select) {
  switch (select) {
    case 0:
      data.remove(0, 4);
      break;
    case 1:
      data.remove(0, data.indexOf("#") + 1);
      data.remove(0, data.indexOf("#"));
      break;
    case 2:
      data.remove(0, data.indexOf("#"));
      break;
    default:
      break;
  }
  return data;
}

// Sets LDR threshold light levels
void Set_ldr(String input) {
  String Settings = Parse_string(input, 1);
  int lightlevel = 0;
  if (input[3] == 'c') {
    lightlevel = analogRead(LDR_pin);
    delay(10);
    lightlevel = analogRead(LDR_pin);
  } else {
    lightlevel = Parse_data(input, 14); 
  }

  for (int i=0; i<4; i++){
    if (LDRs[i].ll == lightlevel){
      delay(1000);
      return;
    }
    if (LDRs[i].ll <= (lightlevel+50) && LDRs[i].ll >= (lightlevel-50)) {
      delay(1000);
      return;
    }
    if (LDRs[i].ll == -1) {
      LDR_set = true;
      digitalWrite(LedPins[8], HIGH);
      LDRs[i].ll = lightlevel;
      LDRs[i].settings = Settings;
      LDRs[i].on = 1;
      delay(1000);
      return;
    }
  }
}

// Sets a timer for given datetime and settings
void Set_timer(String input) {
  int Hour = Parse_data(input, 11);
  int Minute = Parse_data(input, 12);
  int Day = Parse_data(input, 10);
  String Settings = Parse_string(input, 1);

  for (int i=0; i<10; i++){
    if (Timers[i].dag == Day && Timers[i].uur == Hour && Timers[i].minuut == Minute){
      return;
    }
    if (Timers[i].dag == -1) {
      Timers_set = true;
      digitalWrite(LedPins[9], HIGH);
      Timers[i].dag = Day;
      Timers[i].uur = Hour;
      Timers[i].minuut = Minute;
      Timers[i].settings = Settings;
      Timers[i].on = 1;
      return;
    } 
  }
}

int Check_preset(int pos) {
  if (Presets[pos] == "#xxu-t-z-"){
    return 0;
  }
  return 1;
}

String Get_settings() {
  String All_timers;
  for (int i=0; i<10; i++) {
    All_timers += Characters[9] + 'd' + Timers[i].dag + 'h' + Timers[i].uur + 'm' + Timers[i].minuut + 's' + Timers[i].settings + 'b' + Timers[i].on;
  }
  for (int i=0; i<4; i++) {
    All_timers += Characters[9] + 'l' + LDRs[i].ll + 's' + LDRs[i].settings + 'b' + LDRs[i].on;
  }
  return All_timers;
}

// Helper function to handle button actions
void handleButtonPress(int buttonIndex, void (*action)()) {
  if (buttons[buttonIndex].isPressed()) {
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    action();
    digitalWrite(LedPins[0], LOW);
  }
}

void loop() {
  // Check state of all physical buttons
  for (int i = 0; i < 9; i++) {
    buttons[i].loop();
  }
  
  // Button_1 Light switch 1 servo
  handleButtonPress(0, LightSwitch);

  // Button_2 Light switch 2 relay
  handleButtonPress(1, LightSwitch_2);

  // Button_3 Move blinds up 25%
  handleButtonPress(2, Stepper_All(Nextv_stepper(true)));

  // Button_3 Move blinds down 25%
  handleButtonPress(3, Stepper_All(Nextv_stepper(false)));

  // Button_4 Tilt blinds open 25% 
  handleButtonPress(4, Parse_and_execute("#xxu-t" + String(Nextv_servo(true) * 1.5) + "z-"));

  // Button_5 Tilt blinds closed 25%
  handleButtonPress(5, Parse_and_execute("#xxu-t" + String(Nextv_servo(false) * 1.5) + "z-"));

  // Button_6 Start vertical calibration blinds
  handleButtonPress(6, Calibrate_a(););

  // Button_7 Execute preset 4 set by the app
  handleButtonPress(7, Parse_and_execute(Presets[3]));

  // Button_8 Execute preset 8 set by the app
  handleButtonPress(8, Parse_and_execute(Presets[7]));
  
  // Detect audio spikes (1 or 2 claps) to trigger actions
  if (Mic_on) {
    Mic_reading = analogRead(Mic_pin);
    delay(10);
    Mic_reading = analogRead(Mic_pin);

    if (Mic_reading > 530) {
      Serial.print(Mic_reading);
      ClapTrigger();
    }
  }

  // RTC: Check and execute timers at the correct time
  DateTime now = rtc.now();
  if (Tim_on && Timers_set) {
    for (int i = 0; i < 10; i++) {
      if (Timers[i].dag == -1) continue;
      if (Timers[i].dag != now.dayOfTheWeek() && Timers[i].dag != 7) continue;
      if (Timers[i].uur != now.hour() || Timers[i].minuut != now.minute()) continue;
      if (now.second() != 0) continue;
      
      Parse_and_execute(Timers[i].settings);
      delay(1000);
    }
  }

  // LDR: Measures every 5 minutes and checks if the light value falls within the set thresholds
  if (LDR_set && Ldr_on && now.minute() % 5 == 0 && last_ldrtime != now.minute()) {
    last_ldrtime = now.minute();

    LDRValue = analogRead(LDR_pin);

    for (int i = 0; i < 4; i++) {
      if (LDRs[i].on != 1) continue;  // Skip if LDR setting is disabled

      bool withinRange = (LDRs[i].ll >= LDRValue - 50) && (LDRs[i].ll <= LDRValue + 50);

      if (withinRange) {
        if (ldr_status[i] == 0) {
          // First measurement: light value is within the threshold, increment status
          ldr_status[i]++;
        } else {
          // Second measurement within the threshold, execute the action
          Parse_and_execute(LDRs[i].settings);
          ldr_status[i] = 0;
        }
      } else {
        // Light value is no longer within the range, reset the status
        ldr_status[i] = 0;
      }
    }
  }
  
  // Bluetooth Input Handling: Receives commands over Bluetooth and triggers corresponding actions
  if (Serial.available() > 0) {
    digitalWrite(LedPins[0], HIGH); // Turn on activity light
    Incoming_char = Serial.read(); // Read all incoming characters

    // If we encounter a comma, process the command
    if (Incoming_char == ',') {
      if (Incoming_string.length() > 3 && Incoming_string[0] == '#') {
        // Handle Presets: Set or execute
        if (Incoming_string[1] == 'p') {
          // Set a new preset
          if (Incoming_string[2] == 's') {
            Presets[Parse_data(Incoming_string, 3)] = Parse_string(Incoming_string, 0);
            Buzz(1);
          } 
          // Execute the preset
          else {
            Parse_and_execute(Presets[Parse_data(Incoming_string, 3)]);
          }
        } 
        // Toggle microphone, timer, ldr and buzzer on/off
        else if (Incoming_string == "#mic") {
          Mic_on = !Mic_on;
          Buzz(Mic_on ? 1 : 2);
        } 
        else if (Incoming_string == "#ldr") {
          Ldr_on = !Ldr_on;
          Buzz(Ldr_on ? 1 : 2);
        } 
        else if (Incoming_string == "#buz") {
          Buzz_on = !Buzz_on;
          Buzz(5);
        } 
        else if (Incoming_string == "#tim") {
          Tim_on = !Tim_on;
          Buzz(Tim_on ? 1 : 2);
        }
        // Setting microphone actions
        else if (Incoming_string[1] == 's') {
          Mic_functions[Parse_data(Incoming_string, 3)] = Parse_string(Incoming_string, 0);
          Buzz(1);
        } 
        // Setting light threshold and settings
        else if (Incoming_string[2] == 'd') {
          Set_ldr(Incoming_string);
        }
        // Start vertical calibration blinds
        else if (Incoming_string == "#ka0") {
          Calibrate_b();
        }
        // Set RTC with data from app
        else if (Incoming_string[1] == 'r') {
          int Day = Parse_data(Incoming_string, 4);
          int Hour = Parse_data(Incoming_string, 5);
          int Minute = Parse_data(Incoming_string, 6);
          int Year = Parse_data(Incoming_string, 7);
          int Month = Parse_data(Incoming_string, 8);
          int Seconds = Parse_data(Incoming_string, 9);
          rtc.adjust(DateTime(Year, Month, Day, Hour, Minute, Seconds));
        }
        // Fetch status of buttons, position of motors and temperature
        else if (Incoming_string == "#1pull") {
          Serial.print("");
          Serial.print('#' + String(Mic_on) + String(Ldr_on) + String(Buzz_on) + String(Tim_on) + Check_preset(0) + Check_preset(1) + Check_preset(2) + Check_preset(3) + Check_preset(4) + Check_preset(5) + Check_preset(6) + Check_preset(7) + 'u' + (int(double(Stepper_Lux[0])/double(Stepper_Lux[1])*100)) + 't' + int(double(Servo2_pos)*1.5) + 'z' + "100" + 'i' + rtc.getTemperature() + ',');
        }
        // Set new Timer with time and settings
        else if (Incoming_string[1] == 't') {
          Set_timer(Incoming_string);
        }
        // Fetch all settings for LDR and timer
        else if (Incoming_string == "#Top") {
          Serial.print('[');
          Serial.print(Get_settings());
          Serial.print(']');
        }
        // Toggle a specific timer on/off
        else if (Incoming_string[1] == 'q') {
          int timerIndex = Parse_data(Incoming_string, 13);
          Timers[timerIndex].on = !Timers[timerIndex].on;

          // Check if any timer is active and toggle led accordingly
          bool Off = true;
          for (int i = 0; i < 10; i++) {
            if (Timers[i].on == 1) {
              Off = false;
            }
          }
          digitalWrite(LedPins[9], Off ? LOW : HIGH);
        }
        // Clear all timers and turn off timer LED
        else if (Incoming_string[1] == 'w') {
          for (int i = 0; i < 10; i++) {
            Timers[i].dag = -1;
          }
          Timers_set = false;
          digitalWrite(LedPins[9], LOW);
        }
        // Remove a single timer from the list
        else if (Incoming_string[1] == 'c') {
          int timerIndex = Parse_data(Incoming_string, 13);
          Timers[timerIndex].dag = -1;

          // Check if any timers are still active, update the status and LED accordingly
          bool Empty = true;
          for (int i = 0; i < 10; i++) {
            if (Timers[i].dag != -1) {
              Empty = false;
            }
          }
          if (Empty) {
            Timers_set = false;
            digitalWrite(LedPins[9], LOW);
          }
        }
        // Toggle a specific LDR setting and status LED on/off
        else if (Incoming_string[1] == 'v') {
          int ldrIndex = Parse_data(Incoming_string, 13);
          LDRs[ldrIndex].on = !LDRs[ldrIndex].on;

          // Check if any LDR is active
          bool Off = true;
          for (int i = 0; i < 4; i++) {
            if (LDRs[i].on == 1) {
              Off = false;
            }
          }
          digitalWrite(LedPins[8], Off ? LOW : HIGH);
        }
        // Clear all LDR settings and turn off LDR LED
        else if (Incoming_string[1] == 'y') {
          for (int i = 0; i < 4; i++) {
            LDRs[i].ll = -1;
          }
          LDR_set = false;
          digitalWrite(LedPins[8], LOW);
        }
        // Remove a single LDR setting from the list
        else if (Incoming_string[1] == 'z') {
          int ldrIndex = Parse_data(Incoming_string, 13);
          LDRs[ldrIndex].ll = -1;
          bool Empty = true;

          // Check if any LDR is still active, update the status and LED accordingly
          for (int i = 0; i < 4; i++) {
            if (LDRs[i].ll != -1) {
              Empty = false;
            }
          }
          if (Empty) {
            LDR_set = false;
            digitalWrite(LedPins[8], LOW);
          }
        }
        // Reposition blades if stuck :)
        else if (Incoming_string == "#lal0") {
          Reposition();
        } 
        // Set speed of blinds tilt (slow -> quiet)
        else if (Incoming_string == "#lal1") {
          BlindsSpeed();
        }
        // MAIN: Directly execute instructions with general format
        else {
          Parse_and_execute(Incoming_string);
        }
        Incoming_string = ""; // Reset string after processing
      }
    } else {
      Incoming_string += Incoming_char; // Append received character to the string
    }
    digitalWrite(LedPins[0], LOW); // Turn off activity LED
  }
}
