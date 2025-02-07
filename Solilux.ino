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
      Timers[i] = {-1, -1, -1, "", 0};
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

void Parse_and_execute(String code){
  Buzz(0);

  //Light
  if (code[1] == 'l'){  
    Lightswitch();
  }

  if (code[2] == 'l'){  // switch 
    Lightswitch2();
  }

  if (code[2] == 'a'){  // on if off
    Lightswitch2_1();
  }

  if (code[2] == 'o'){  // off if on
    Lightswitch2_2();
  }
  
  //Luxaflex tilt
  current_Parsed = Parse_data(code, 1);
  if (current_Parsed != -1) {
    if (current_Parsed/1.5 != Servo2_pos) {
      Servo2.attach(Servo2_pin);
  
      if (Sspeed) {
        Servo2.write(current_Parsed/1.5);
        Servo2_pos = current_Parsed/1.5;
        delay(2000);
      } else {
        if (current_Parsed/1.5 > Servo2_pos) {
          for (Servo2_pos; Servo2_pos < current_Parsed/1.5; Servo2_pos += 1) {
            Servo2.write(Servo2_pos);             
            delay(50); // waits 15 ms for the servo to reach the position
          }       
        } else {
          for (Servo2_pos; Servo2_pos > current_Parsed/1.5; Servo2_pos -= 1) {
            Servo2.write(Servo2_pos);             
            delay(50); // waits 15 ms for the servo to reach the position
          }
        }
      }
      Servo2.detach();
      Serial.write(current_Parsed);
      if (current_Parsed == 0) {
        digitalWrite(LedPins[4], LOW);
        digitalWrite(LedPins[5], HIGH);
      } else if (current_Parsed == 270) {
        digitalWrite(LedPins[5], LOW);
        digitalWrite(LedPins[4], HIGH);
      } else {
        digitalWrite(LedPins[4], LOW);
        digitalWrite(LedPins[5], LOW);
      }
    }
  }

  // Luxaflex hight
  current_Parsed = Parse_data(code, 0);
  if (current_Parsed != -1) {
    Stepper_All(current_Parsed);
    Serial.write(current_Parsed);
  }
}

int Parse_data(String data, int select){
  //select 0 = luxaflex, 1 = tilt, 2 = zonwering
  if (select == 0){
    data.remove(0, data.indexOf("u")+1);
    data.remove(data.indexOf("t"));
    if (data == "-"){
      return -1;
    }
    return data.toInt();
  }
  if (select == 1){
    data.remove(0, data.indexOf("t")+1);
    data.remove(data.indexOf("z"));
    if (data == "-") {
      return -1;
    }
    return data.toInt();
  }
  if (select == 2){
    data.remove(0, data.indexOf("z")+1);
    if (data == "-") {
      return -1;
    }
    return data.toInt();
  }
  if (select == 3){
    data.remove(0, 3);
    data.remove(data.indexOf("#"));
    return data.toInt();
  }
  if (select == 4){
    data.remove(0, data.indexOf("!")+1);
    data.remove(data.indexOf("@"));
    Serial.println(data);
    return data.toInt();
  }
  if (select == 5){
    data.remove(0, data.indexOf("*")+1);
    data.remove(data.indexOf("$"));
    Serial.println(data);
    return data.toInt();
  }
  if (select == 6){
    data.remove(0, data.indexOf("$")+1);
    data.remove(data.indexOf("/"));
    Serial.println(data);
    return data.toInt();
  }
  if (select == 7){
    data.remove(0, data.indexOf("@")+1);
    data.remove(data.indexOf("*"));
    Serial.println(data);
    return data.toInt();
  }
  if (select == 8){
    data.remove(0, data.indexOf("c")+1);
    data.remove(data.indexOf("!"));
    Serial.println(data);
    return data.toInt();
  }
  if (select == 9){
    data.remove(0, data.indexOf("/")+1);
    Serial.println(data);
    return data.toInt();
  }
  if (select == 10){
    data.remove(0, data.indexOf("t")+1);
    data.remove(data.indexOf("."));
    return data.toInt();
  }
  if (select == 11){
    data.remove(0, data.indexOf(".")+1);
    data.remove(data.indexOf("."));
    return data.toInt();
  }
  if (select == 12){
    data.remove(0, data.indexOf(".")+1);
    data.remove(0, data.indexOf(".")+1);
    data.remove(data.indexOf("#"));
    return data.toInt();
  }
  if (select == 13){
    data.remove(0, data.indexOf("m")+1);
    return data.toInt();
  }
  if (select == 14){
    data.remove(0, data.indexOf("n")+1);
    data.remove(data.indexOf("#"));
    return data.toInt();
  }
}

void Mic() {
   Clapcount++;
   delay(500);
   for (int i=0; i<500; i++) {
     Mic_reading = analogRead(Mic_pin);
     delay(1);
     if (Mic_reading > 600) {
       Clapcount++;
     }
   }
   if (Clapcount == 1) {
     Buzz(0);
     Serial.println("1");
     Parse_and_execute(Mic_functions[0]);
   } else {
     Buzz(1);
     Serial.println("2");
     Parse_and_execute(Mic_functions[1]);
   }
   Clapcount = 0;
}

void Speed() {
  if (Sspeed) {
    Sspeed = false; //traag
    digitalWrite(LedPins[10], HIGH);
    digitalWrite(LedPins[11], LOW);
  } else {
    Sspeed = true; //snel
    digitalWrite(LedPins[11], HIGH);
    digitalWrite(LedPins[10], LOW);
  }
}

void Lightswitch() {
  for (Servo1_pos = 0; Servo1_pos <= 120; Servo1_pos += 1) { 
    // in steps of 1 degree
    Servo1.write(Servo1_pos);             
    delay(10);                 
  }
  for (Servo1_pos = 120; Servo1_pos >= 0; Servo1_pos -= 1) { 
    Servo1.write(Servo1_pos);              
    delay(5);                      
  }
}

void Lightswitch2() {
  Serial.println(Relay_state);
  if (Relay_state) {
    digitalWrite(Relay_pin, HIGH);
    Relay_state = false;
  } else {
    digitalWrite(Relay_pin, LOW);
    Relay_state = true;
  }
  // switch relay light 2
}

void Lightswitch2_1() {
  digitalWrite(Relay_pin, LOW);
  Relay_state = true;
  //turn on, if it already is or not
}

void Lightswitch2_2() {
  digitalWrite(Relay_pin, HIGH);
  Relay_state = false;
  //turn off, if it already is or not
}

int Nextv_stepper(bool dir) {
  // true = +, false = -
  Serial.println("---------value / max-------");
  int value = ((double)Stepper_Lux[0]/(double)Stepper_Lux[1])*100;
  Serial.println(value);
  Serial.println("---------value / max----0-0----");

  if (dir) {
    for (int i=0; i<5; i++) {
      if (Step_25[i] > value) {
        return Step_25[i];
      }
    }
    return 100;
  }
  Serial.println("begin door array heengaan");
  for (int i=4; i>=0; i--) { //reversed search from last index to 0
    Serial.println(Step_25[i]);
    if (Step_25[i] < value) {
      return Step_25[i];
      Serial.println("einde door array heengaan");
    }
  }
  return 0;
}

int Nextv_servo(bool dir) {
  // true = +, false = -
  if (dir) {
    for (int i=0; i<5; i++) {
      if (Servo_25[i] > Servo2_pos) {
        return Servo_25[i];
      }     
    }
    return 180; 
  }
  for (int i=4; i>=0; i--) { //reversed search from last index to 0
    if (Servo_25[i] < Servo2_pos) {
      return Servo_25[i];
    }
  }
  return 0;
}

void Stepper_All(int pos) {
  //Update leds
  if (pos == 0) {
    digitalWrite(LedPins[2], LOW);
    digitalWrite(LedPins[3], HIGH);
  } else if (pos == 100) {
    digitalWrite(LedPins[3], LOW);
    digitalWrite(LedPins[2], HIGH);
  } else {
    digitalWrite(LedPins[2], LOW);
    digitalWrite(LedPins[3], LOW);
  }
    
  //Check verkeerde input
  if (pos<0 || pos>100) {
    return;
  }
  
  //Graden -> steps
  InputStep = ((double)pos/(double)100)*Stepper_Lux[1];
  Serial.println(InputStep);
  Serial.println("--------");
  Serial.println(Stepper_Lux[1]);
  if (InputStep==Stepper_Lux[0]) {
    return;
  }
  if (InputStep>Stepper_Lux[0]) {
    digitalWrite(Stepper_Lux[3], LOW);
    TakeSteps = InputStep - Stepper_Lux[0];
  } else {
    digitalWrite(Stepper_Lux[3], HIGH);
    TakeSteps = Stepper_Lux[0] - InputStep;
  }
  
  digitalWrite(StepOn, LOW);
  for (int j = 0; j < 1000; j++) {
    for (int i = 0; i < TakeSteps; i++) {
      digitalWrite(Stepper_Lux[4], HIGH);
      delayMicroseconds(15);
      digitalWrite(Stepper_Lux[4], LOW);
      delayMicroseconds(15);
    }
  }
  Stepper_Lux[0] = InputStep;
  digitalWrite(StepOn, HIGH);
}

void Buzz(int select){
  if (Buzz_on || select==5 || select==6) {
    tone(Buzzer, 1000, 50);
    if (select == 1) {
      delay(100);
      tone(Buzzer, 1000, 50);
    } else if (select == 2) {
      delay(100);
      tone(Buzzer, 1000, 50);
      delay(100);
      tone(Buzzer, 1000, 50);
    } else if (select == 3) {
      delay(100);
      tone(Buzzer, 1000, 50);
      delay(100);
      tone(Buzzer, 1000, 50);
      delay(100);
      tone(Buzzer, 1000, 50);
    } else if (select == 4) {
      tone(Buzzer, 500, 400);
    } else if (select == 5) {
      delay(100);
      tone(Buzzer, 1000, 50);
    } else if (select == 6) {
      delay(100);
      tone(Buzzer, 1000, 50);
      delay(100);
      tone(Buzzer, 1000, 50);
    }
  }
}

void Reposition() {
  Servo2.attach(Servo2_pin);
  if (Sspeed) {
    if (Servo2_pos >= 90) {
      Servo2.write(0);
      delay(2000);
      Servo2.write(Servo2_pos);
      delay(2000);
    } else {
      Servo2.write(180);
      delay(2000);
      Servo2.write(Servo2_pos);
      delay(2000);
    }
  } else {
    if (Servo2_pos >= 90) {
      for (int i=Servo2_pos; i>0; i--) {
        Servo2.write(i);             
        delay(50);                       // waits 15 ms for the servo to reach the position
      }       
      for (int i=0; i<Servo2_pos; i++) {
        Servo2.write(i);             
        delay(50);                       // waits 15 ms for the servo to reach the position
      }       
    } else {
      for (int i=Servo2_pos; i<180; i++) {
        Servo2.write(i);             
        delay(50);                       // waits 15 ms for the servo to reach the position
      }
      for (int i=180; i>Servo2_pos; i--) {
        Servo2.write(i);             
        delay(50);                       // waits 15 ms for the servo to reach the position
      }  
    }
  }
  Servo2.detach();
}

String Parse_string(String data, int select) {
  if (select == 0) {
    data.remove(0, 4);
    return data;
  }
  if (select == 1) {
    data.remove(0, data.indexOf("#")+1);
    data.remove(0, data.indexOf("#"));
    return data;
  }
  if (select == 2) {
    data.remove(0, data.indexOf("#"));
    return data;
  }
}

int Check_preset(int pos) {
  if (Presets[pos] == "#xxu-t-z-"){
    return 0;
  }
  return 1;
}

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

  int Empty = true;
  for (int i=0; i<4; i++){
    if (LDRs[i].ll == lightlevel){
      Empty = false;
      Serial.print("f0");
      delay(1000);
      return;
    }
    if (LDRs[i].ll <= (lightlevel+50) && LDRs[i].ll >= (lightlevel-50)) {
      Serial.print("f3");
      delay(1000);
      return;
    }
    if (LDRs[i].ll == -1) {
      Empty = false;
      LDR_set = true;
      digitalWrite(LedPins[8], HIGH);
      LDRs[i].ll = lightlevel;
      LDRs[i].settings = Settings;
      LDRs[i].on = 1;
      Serial.print("f1");
      Serial.print(lightlevel);
      delay(1000);
      return;
    }
  }
  if (Empty) {
    Serial.print("f2");
    delay(1000);
  }
}

void Set_timer(String input) {
  int Hour = Parse_data(input, 11);
  int Minute = Parse_data(input, 12);
  int Day = Parse_data(input, 10);
  String Settings = Parse_string(input, 1);
  int Empty = true;
  for (int i=0; i<10; i++){
    if (Timers[i].dag == Day && Timers[i].uur == Hour && Timers[i].minuut == Minute){
      Empty = false;
      Serial.print("f0");
      return;
    }
    if (Timers[i].dag == -1) {
      Empty = false;
      Timers_set = true;
      digitalWrite(LedPins[9], HIGH);
      Timers[i].dag = Day;
      Timers[i].uur = Hour;
      Timers[i].minuut = Minute;
      Timers[i].settings = Settings;
      Timers[i].on = 1;
      Serial.print("f1");
      return;
    } 
  }
  if (Empty) {
    Serial.print("f2");
  }
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

void loop() {
  // Check state of all physical buttons
  for (int i = 0; i < 9; i++) {
    buttons[i].loop();
  }
  
  //Lightswitch 1
  if(buttons[0].isPressed()) {
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    Lightswitch();
    digitalWrite(LedPins[0], LOW);
  }
  //Preset button 4
  if(buttons[1].isPressed()) {
    Serial.println("37");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    Lightswitch2();
    digitalWrite(LedPins[0], LOW);
  }
  //Preset button 8
  if(buttons[2].isPressed()) {
    Serial.println("38");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    int val = Nextv_stepper(true);
    Stepper_All(val);
    digitalWrite(LedPins[0], LOW);
  }
  //Lightbutton
  if(buttons[3].isPressed()) {
    Serial.println("39");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    int val = Nextv_stepper(false);
    Stepper_All(val);
    digitalWrite(LedPins[0], LOW);
  }
  //Lux up 25%
  if(buttons[4].isPressed()) {
    Serial.println("40");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    int val = Nextv_servo(true) * 1.5;
    Serial.println(val);
    Parse_and_execute("#xxu-t" + String(val) + "z-");
    digitalWrite(LedPins[0], LOW);
  }

  //Tilt open 25%
  if(buttons[5].isPressed()) {
    Serial.println("41");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    int val = Nextv_servo(false) * 1.5;
    Serial.println(val);
    Parse_and_execute("#xxu-t" + String(val) + "z-");
    digitalWrite(LedPins[0], LOW);
  }
  
  //Sun screen
   if(buttons[6].isPressed()) {
    Serial.println("42");
    Buzz(4);
    digitalWrite(LedPins[0], HIGH);
    Calibrate_a();
    Buzz(4);
    digitalWrite(LedPins[0], LOW);
   }
   
  //Lux down 25%
  if(buttons[7].isPressed()) {
    Serial.println("43");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    Parse_and_execute(Presets[3]);
    digitalWrite(LedPins[0], LOW);
  }
  //Tilt close 25%
  if(buttons[8].isPressed()) {
    Serial.println("45");
    Buzz(0);
    digitalWrite(LedPins[0], HIGH);
    Parse_and_execute(Presets[7]);
    digitalWrite(LedPins[0], LOW);
  }
  
  //Microfone
 if (Mic_on) {
   Mic_reading = analogRead(Mic_pin);
   delay(10);
   Mic_reading = analogRead(Mic_pin);
   Serial.println(Mic_reading);
   if (Mic_reading > 530) {
     Serial.print(Mic_reading);
     Mic();
   }
 }

  //RTC
  DateTime now = rtc.now();
  if (Tim_on) {
    if (Timers_set) {
        for (int i=0; i<10; i++) {
            if (Timers[i].dag != -1) {
                if (Timers[i].dag == now.dayOfTheWeek() || Timers[i].dag == 7) {
                    if (Timers[i].uur == now.hour()) {
                        if (Timers[i].minuut == now.minute()) {
                            if (now.second() == 0) {
                              Serial.println("juiste_minuut en seconde 0, Timer uitvoeren");
                              Serial.println(Timers[i].settings);
                              Parse_and_execute(Timers[i].settings);
                              delay(1000);
                            }
                        }
                    }
                }
            }
        }
    }
  }

  //LDR
  if (LDR_set) {
    if (Ldr_on) {
      if (now.minute() % 5 == 0 && last_ldrtime != now.minute()) {
        last_ldrtime = now.minute();
        Serial.println(now.minute());
        LDRValue = analogRead(LDR_pin);
        Serial.println(LDRValue);
        for (int i=0; i<4; i++) {
          if (LDRs[i].on == 1) {
            if (LDRs[i].ll <= (LDRValue+50) && LDRs[i].ll >= (LDRValue-50)) {
              if (ldr_status[i] == 0) {
                Serial.println("begint controle: nul naar 1");
                ldr_status[i]++;
              } else {
                Serial.println("tweede keer zelfde waarde: 1 naar 2 naar nul");
                Parse_and_execute(LDRs[i].settings);
                ldr_status[i] = 0;
                LDRs[i].on = 0;
              }
            } else {
              Serial.println("waarde vervallen waarde varanderd, 1 naar nul");
              ldr_status[i] = 0;
            }
          }
        }
      }
    }
  }
  
  //Bluetooth input
  if (Serial.available()>0) {
    digitalWrite(LedPins[0], HIGH); 
    Incoming_char = Serial.read();
    if (Incoming_char == ',') {
      if (Incoming_string.length() > 3 && Incoming_string[0] == '#') {
        
        Serial.println(Incoming_string);

        //Presets (set and execute)
        if (Incoming_string[1] == 'p'){
          if (Incoming_string[2] == 's'){
            Presets[Parse_data(Incoming_string, 3)] = Parse_string(Incoming_string, 0);
            Buzz(1);
          } else {
            Parse_and_execute(Presets[Parse_data(Incoming_string, 3)]);
          }
        } 

        //Booleans
        //Microfone
        else if (Incoming_string == "#mic") {
          if (Mic_on) {
            Mic_on = false;
            Buzz(2);
          } else {
            Mic_on = true;
            Buzz(1);
          }
        //Ldr
        } else if (Incoming_string == "#ldr") {
          if (Ldr_on) {
            Ldr_on = false;
            Buzz(2);
          } else {
            Ldr_on = true;
            Buzz(1);
          }
        //Buzzer
        } else if (Incoming_string == "#buz") {
          if (Buzz_on) {
            Buzz_on = false;
            Buzz(6);
          } else {
            Buzz_on = true;
            Buzz(5);
          }
        //Timer
        } else if (Incoming_string == "#tim") {
          if (Tim_on) {
            Tim_on = false;
            Buzz(2);
          } else {
            Tim_on = true;
            Buzz(1);
          }
        }
        
        //Set microfone functions
        else if (Incoming_string[1] == 's') {
          Mic_functions[Parse_data(Incoming_string, 3)] = Parse_string(Incoming_string, 0);
          Buzz(1);
        //Set LDR fuctions
        } else if (Incoming_string[2] == 'd') {
          Set_ldr(Incoming_string);
        //Calibrate b
        } else if (Incoming_string == "#ka0") {
          Calibrate_b();
        //Set RTC
        } else if (Incoming_string[1] == 'r') {
          int Day = Parse_data(Incoming_string, 4);
          int Hour = Parse_data(Incoming_string, 5);
          int Minute = Parse_data(Incoming_string, 6);
          int Year = Parse_data(Incoming_string, 7);
          int Month = Parse_data(Incoming_string, 8);
          int Seconds = Parse_data(Incoming_string, 9);
          rtc.adjust(DateTime(Year, Month, Day, Hour, Minute, Seconds));
          Serial.println("DONE");
        //Pull status buttons, position motors and temparature RTC
        } else if (Incoming_string == "#1pull") {
          Serial.print("");
          Serial.print('#' + String(Mic_on) + String(Ldr_on) + String(Buzz_on) + String(Tim_on) + Check_preset(0) + Check_preset(1) + Check_preset(2) + Check_preset(3) + Check_preset(4) + Check_preset(5) + Check_preset(6) + Check_preset(7) + 'u' + (int(double(Stepper_Lux[0])/double(Stepper_Lux[1])*100)) + 't' + int(double(Servo2_pos)*1.5) + 'z' + "100" + 'i' + rtc.getTemperature() + ',');
        //Set timer
        } else if (Incoming_string[1] == 't') {
          Set_timer(Incoming_string);
        //Pull settings
        } else if (Incoming_string == "#Top") {
          Serial.print('[');
          Serial.print(Get_settings());
          Serial.print(']');
        //Set Timer on/off
        } else if (Incoming_string[1] == 'q') {
          if (Timers[Parse_data(Incoming_string, 13)].on == 1) {
            Timers[Parse_data(Incoming_string, 13)].on = 0;
          } else {
            Timers[Parse_data(Incoming_string, 13)].on = 1;
          }
          bool Off = true;
          for (int i=0; i<10; i++) {
            if (Timers[i].on == 1) {
              Off = false;
            }
          }
          if (Off) {
            digitalWrite(LedPins[9], LOW);
          } else {
            digitalWrite(LedPins[9], HIGH);
          }
        //Empty timers list
        } else if (Incoming_string[1] == 'w') {
          for (int i=0; i<10; i++) {                             
            Timers[i].dag = -1; //mit app check alleen de dag
          }
          Timers_set = false;
          digitalWrite(LedPins[9], LOW);
        //Remove single timer
        } else if (Incoming_string[1] == 'c') {
          Timers[Parse_data(Incoming_string, 13)].dag = -1; //mit app check alleen de dag
          bool Empty = true;
          for (int i=0; i<10; i++) {
            if (Timers[i].dag != -1) {
              Empty = false;
            }
          }
          if (Empty) {
            Timers_set = false;
            digitalWrite(LedPins[9], LOW);
          }
        //Set ldr on/off
        } else if (Incoming_string[1] == 'v') {
          if (LDRs[Parse_data(Incoming_string, 13)].on == 1) {
            LDRs[Parse_data(Incoming_string, 13)].on = 0;
          } else {
            LDRs[Parse_data(Incoming_string, 13)].on = 1;
          }
          bool Off = true;
          for (int i=0; i<4; i++) {
            if (LDRs[i].on == 1) {
              Off = false;
            }
          }
          if (Off) {
            digitalWrite(LedPins[8], LOW);
          } else {
            digitalWrite(LedPins[8], HIGH);
          }
        //Empty ldr list
        } else if (Incoming_string[1] == 'y') {
          for (int i=0; i<4; i++) {
            LDRs[i].ll = -1; //mit app check alleen lightlevel
          }
          LDR_set = false;
          digitalWrite(LedPins[8], LOW);
        //Remove single ldr
        } else if (Incoming_string[1] == 'z') {
          LDRs[Parse_data(Incoming_string, 13)].ll = -1; //mit app check alleen de dag
          int Empty = true;
          for (int i=0; i<4; i++) {
            if (LDRs[i].ll != -1) {
              Empty = false;
            }
          }
          if (Empty) {
            LDR_set = false;
            digitalWrite(LedPins[8], LOW);
          }
        } else if (Incoming_string == "#lal0") {
          Reposition();
          Serial.println("recalibrate tilt");
        } else if (Incoming_string == "#lal1") {
          Speed();
          Serial.println("speed servo");
        } else { //Overig: uitvoeren van opdracht (format: '#llu50t50z50')
          Parse_and_execute(Incoming_string);
        }
        Incoming_string = "";
      }
    }
    else {
      Incoming_string += Incoming_char;
    }
    digitalWrite(LedPins[0], LOW);
  }
}
