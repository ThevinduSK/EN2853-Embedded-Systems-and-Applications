#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET_DST 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire , OLED_RESET);
DHTesp dhtSensor;

// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int month = 0;
float UTC_OFFSET = 0.0; 

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
int n_alarms = 2;
int alarm_hours[] = {0,1};
int alarm_minutes[] = {1,10};
bool alarm_triggered[] = {false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 7;
String modes[] = { "1 - Set Time",
                   "2 - Set Alarm 1", 
                   "3 - Set Alarm 2", 
                   "4 - Disable Alarms", 
                   "5 - Set Timezone", 
                   "6 - View Alarms", 
                   "7 - Delete Alarm"};

void print_line(String text, int column, int row, int text_size);
void update_time_with_check_alarm(void);
void go_to_menu();
void check_temp();
void run_mode(int mode);
void set_timezone();

void setup() {

  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.display();
  delay(500);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);

  configTime((int)(UTC_OFFSET * 3600), UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();
  display.display();

  print_line("Welcome to Medibox!", 10, 20, 2);
  delay(500);
  display.clearDisplay();
}

void loop() {
  update_time_with_check_alarm();
  if (digitalRead(PB_OK) == LOW){
    delay(200);
    go_to_menu();
  }
  check_temp();
}

void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text); 

  display.display();
}

void print_time_now(void) {
  display.clearDisplay();  // 

  //show time
  String timeStr = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
  print_line(timeStr, 0, 0, 2);
  
  // Show date (DD/MM)
  String dateStr = String(days) + "/" + String(month);
  print_line(dateStr, 0, 25, 2); //Shows below the time

  display.display();
}

void update_time() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    hours = timeinfo.tm_hour;
    minutes = timeinfo.tm_min;
    seconds = timeinfo.tm_sec;
    days = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1; // tm_mon is 0-11, so we add 1
  } else {
    Serial.println("Failed to get time from NTP.");
  }
}

void ring_alarm(){
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);

  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  //ring the buzzer
  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH){
    for (int i=0; i<n_notes; i++){
      if (digitalRead(PB_CANCEL) == LOW){
        delay(200);
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }
    

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

void update_time_with_check_alarm(void){
  update_time();
  print_time_now();

  if (alarm_enabled == true){
    for (int i=0; i<n_alarms; i++){
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes){
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

int wait_for_button_press(){
  while(true){
    if (digitalRead(PB_UP) == LOW){
      delay(200);
      return PB_UP;
    }

    else if (digitalRead(PB_DOWN) == LOW){
      delay(200);
      return PB_DOWN;
    }
    
    else if (digitalRead(PB_OK) == LOW){
      delay(200);
      return PB_OK;
    }

    else if (digitalRead(PB_CANCEL) == LOW){
      delay(200);
      return PB_CANCEL;
    }

    update_time();
  }
}

void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH){
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }

    else if (pressed == PB_DOWN){
      delay(200);
      current_mode -= 1;
      if (current_mode<0){
        current_mode = max_modes-1;
      }
    }

    else if (pressed == PB_OK){
      delay(200);
      Serial.println(current_mode);
      run_mode(current_mode);
    }

    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }
}

void set_time() {

  int temp_hour = hours;
  while(true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0){
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK){
      delay(200);
      hours = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  int temp_minute = minutes;
  while(true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0){
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK){
      delay(200);
      minutes = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Time is set", 0, 0, 2);
  delay(1000);
}

void set_alarm(int alarm){

  int temp_hour = alarm_hours[alarm];
  while(true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0){
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK){
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while(true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0){
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK){
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);

}

void run_mode(int mode) {
  if (mode == 0){
    set_time();
  }

  if (mode == 1 || mode == 2){
    set_alarm(mode - 1);
  }

  else if (mode == 3){
    alarm_enabled = false;
  }

  else if (mode == 4){
    set_timezone();
  }
}

void check_temp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (data.temperature > 35){
    display.clearDisplay();
    print_line("TEMP HIGH", 0, 40, 1);
  }
  else if (data.temperature < 25){
    display.clearDisplay();
    print_line("TEMP LOW", 0, 40, 1);
  }
  
  if (data.humidity > 40){
    display.clearDisplay();
    print_line("HUMIDITY HIGH", 0, 50, 1);
  }
  else if (data.humidity < 20){
    display.clearDisplay();
    print_line("HUMIDITY LOW", 0, 50, 1);
  }

}

void set_timezone() {
  int sign = (UTC_OFFSET >= 0) ? 1 : -1;
  int int_part = abs((int)UTC_OFFSET);
  float decimal_part = abs(UTC_OFFSET) - int_part;

  int decimal_step = (int)(decimal_part * 100); // example: 0.5 → 50

  int current_step = 0;
  String steps[] = {"Sign", "Hour", "Decimal"};

  while (true) {
    display.clearDisplay();
    String line = "UTC Offset: ";
    if (sign < 0) line += "-"; else line += "+";
    line += String(int_part);
    line += ".";
    if (decimal_step < 10) line += "0"; // pad 0 if needed
    line += String(decimal_step);

    print_line(line, 0, 0, 2);
    print_line("Setting: " + steps[current_step], 0, 30, 1);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      if (current_step == 0) {
        sign *= -1;
      } else if (current_step == 1) {
        int_part = (int_part + 1) % 15;  // UTC range is -14 to +14
      } else if (current_step == 2) {
        decimal_step += 25;
        if (decimal_step > 75) decimal_step = 0;
      }
    }

    else if (pressed == PB_DOWN) {
      if (current_step == 0) {
        sign *= -1;
      } else if (current_step == 1) {
        int_part -= 1;
        if (int_part < 0) int_part = 14;
      } else if (current_step == 2) {
        decimal_step -= 25;
        if (decimal_step < 0) decimal_step = 75;
      }
    }

    else if (pressed == PB_OK) {
      current_step++;
      if (current_step >= 3) {
        float final_offset = sign * (int_part + (decimal_step / 100.0));
        UTC_OFFSET = final_offset;

        configTime((int)(UTC_OFFSET * 3600), UTC_OFFSET_DST, NTP_SERVER);

        display.clearDisplay();
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
          print_line("Time Sync Failed", 0, 0, 1);
        } else {
          print_line("Timezone Set", 0, 0, 2);
        }
        delay(1000);
        break;
      }
    }

    else if (pressed == PB_CANCEL) {
      break;
    }
  }
}

