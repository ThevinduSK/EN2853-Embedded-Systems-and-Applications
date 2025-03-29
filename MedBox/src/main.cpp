#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <Preferences.h>

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

Preferences prefs;

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
bool alarm_repeat[] = {true, true};  // true = repeat daily, false = one-time


// Snooze functionality
bool snooze_active = false;
unsigned long snooze_start_time = 0;
int snooze_alarm_id = -1;
const unsigned long SNOOZE_DURATION = 5 * 60 * 1000; // 5 minutes in milliseconds

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

// Icon bitmaps (8x8)
const unsigned char alarm_on_icon [] PROGMEM = {
  0b01111110,
  0b10000001,
  0b10100101,
  0b10011001,
  0b10011001,
  0b10100101,
  0b10000001,
  0b01111110
};


const unsigned char alarm_off_icon [] PROGMEM = {
  0b00111100,
  0b01000010,
  0b10100101,
  0b10011001,
  0b10000001,
  0b10111101,
  0b01000010,
  0b00111100
};

const unsigned char thermometer_icon [] PROGMEM = {
  0b00110000,
  0b01001000,
  0b10000100,
  0b10000100,
  0b10000100,
  0b10000100,
  0b01001000,
  0b00110000
};

const unsigned char droplet_icon [] PROGMEM = {
  0b00010000,
  0b00111000,
  0b01111100,
  0b01111100,
  0b01111100,
  0b00111000,
  0b00010000,
  0b00000000
};

void draw_main_display();
void print_line(String text, int column, int row, int text_size);
void update_time_with_check_alarm();
void go_to_menu();
void check_temp();
void run_mode(int mode);
void set_timezone();
void update_time();
void delete_alarm();
void view_alarms();
void ring_alarm();
void save_settings();
void draw_icon(const unsigned char *icon, int x, int y) {
  display.drawBitmap(x, y, icon, 8, 8, WHITE);
}



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

  // Connect to Wi-Fi
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);

  // Load saved settings from EEPROM
  prefs.begin("medibox", true); // true = read-only
  UTC_OFFSET = prefs.getFloat("tz_offset", 0.0);  // default 0.0
  alarm_enabled = prefs.getBool("alarm_en", true);
  for (int i = 0; i < n_alarms; i++) {
    alarm_hours[i] = prefs.getInt(("a_hr" + String(i)).c_str(), 0);
    alarm_minutes[i] = prefs.getInt(("a_min" + String(i)).c_str(), 0);
    alarm_triggered[i] = false; // always reset on boot
    alarm_repeat[i] = prefs.getBool(("a_rep" + String(i)).c_str(), true);

  }
  prefs.end();

  // Configure time with loaded timezone
  configTime((int)(UTC_OFFSET * 3600), UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();
  print_line("Welcome to Medibox!", 10, 20, 2);
  delay(500);
  display.clearDisplay();
}


void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - timeLast >= 1000) {
    timeLast = currentMillis;
    update_time();
    draw_main_display();
  }

  // Check if snooze timer has elapsed
  if (snooze_active && (currentMillis - snooze_start_time >= SNOOZE_DURATION)) {
    snooze_active = false;
    ring_alarm(); // Ring alarm again after snooze
  }

  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }

  // Only check temperature every 2 seconds
  static unsigned long lastTempCheck = 0;
  if (currentMillis - lastTempCheck >= 2000) {
    lastTempCheck = currentMillis;
    check_temp();
  }
  
  // Check for alarms
  if (alarm_enabled) {
    for (int i=0; i<n_alarms; i++){
      if (!alarm_triggered[i] && alarm_hours[i] == hours && alarm_minutes[i] == minutes && seconds < 10){
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}




void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

void draw_main_display() {
  display.clearDisplay();

  // Time (top)
  String timeStr = (hours < 10 ? "0" : "") + String(hours) + ":" +
                   (minutes < 10 ? "0" : "") + String(minutes) + ":" +
                   (seconds < 10 ? "0" : "") + String(seconds);
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.setTextColor(WHITE);
  display.println(timeStr);

  // Alarm icon (top right)
  if (alarm_enabled) {
    draw_icon(alarm_on_icon, 105, 0);  // You can adjust (x, y) if needed
  }

  // Date and Timezone
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Date: " + String(days) + "/" + String(month));
  display.setCursor(80, 30);
  display.print("TZ: " + String(UTC_OFFSET, 1));

  // Temp and humidity
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  // Temp icon + value
  draw_icon(thermometer_icon, 0, 40);
  display.setCursor(10, 40);
  display.print(data.temperature, 1);
  display.print("C");

  // Humidity icon + value
  draw_icon(droplet_icon, 60, 40);
  display.setCursor(70, 40);
  display.print(data.humidity, 0);
  display.print("%");

  // Snooze countdown
  if (snooze_active) {
    unsigned long time_remaining = SNOOZE_DURATION - (millis() - snooze_start_time);
    int remaining_minutes = time_remaining / 60000;
    int remaining_seconds = (time_remaining % 60000) / 1000;

    display.setCursor(0, 55);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("Snoozed: ");
    if (remaining_minutes < 10) display.print("0");
    display.print(remaining_minutes);
    display.print(":");
    if (remaining_seconds < 10) display.print("0");
    display.print(remaining_seconds);
  }

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

void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  print_line("OK=Dismiss CANCEL=Snooze", 0, 40, 1);

  digitalWrite(LED_1, HIGH);

  unsigned long alarmStartTime = millis();
  bool alarmSnoozed = false;

  // Ring for at most 30 seconds if no button is pressed
  while (millis() - alarmStartTime < 30000) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        alarmSnoozed = true;
        break;
      }
      else if (digitalRead(PB_OK) == LOW) {
        delay(200);
        digitalWrite(LED_1, LOW);
        display.clearDisplay();
        return; // Dismiss alarm completely
      }

      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }

    if (alarmSnoozed) {
      break;
    }
  }

  digitalWrite(LED_1, LOW);

  if (alarmSnoozed) {
    // Set a snooze for 5 minutes
    display.clearDisplay();
    print_line("Alarm snoozed", 0, 0, 2);
    print_line("for 5 minutes", 0, 20, 2);
    delay(2000);

    snooze_active = true;
    snooze_start_time = millis();
  }

  // Mark one-time alarms as triggered
  for (int i = 0; i < n_alarms; i++) {
    if (alarm_hours[i] == hours &&
        alarm_minutes[i] == minutes &&
        !alarm_repeat[i]) {
      alarm_triggered[i] = true;
    }
  }

  display.clearDisplay();
}


void update_time_with_check_alarm(void){
  update_time();
  draw_main_display();

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


void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour = (temp_hour + 1) % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) temp_hour = 23;
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      return;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute = (temp_minute + 1) % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) temp_minute = 59;
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      return;
    }
  }

  // Ask if this alarm should repeat
  bool repeat = alarm_repeat[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Repeat daily?", 0, 0, 2);
    print_line(repeat ? "Yes" : "No", 0, 30, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP || pressed == PB_DOWN) {
      repeat = !repeat;
    }

    else if (pressed == PB_OK) {
      alarm_repeat[alarm] = repeat;
      break;
    }

    else if (pressed == PB_CANCEL) {
      break;
    }
  }

  // Save everything to EEPROM
  save_settings();

  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);
}


void run_mode(int mode) {
  if (mode == 0){
    set_time();
  }
  else if (mode == 1 || mode == 2){
    set_alarm(mode - 1);
  }
  else if (mode == 3){
    // Toggle alarm state
    alarm_enabled = !alarm_enabled;
    save_settings();
    display.clearDisplay();
    print_line("Alarms " + String(alarm_enabled ? "enabled" : "disabled"), 0, 0, 2);
    delay(1500);
  }
  else if (mode == 4){
    set_timezone();
  }
  else if (mode == 5){
    view_alarms();
  }
  else if (mode == 6){
    delete_alarm();
  }
}

void check_temp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  bool warning = false;
  
  // Update with correct healthy ranges
  // Healthy Temperature: 24°C ≤ Temperature ≤ 32°C
  // Healthy Humidity: 65% ≤ Humidity ≤ 80%
  if (data.temperature > 32 || data.temperature < 24 || 
      data.humidity > 80 || data.humidity < 65) {
    
    // Clear just the warning area at the bottom
    display.fillRect(0, 50, SCREEN_WIDTH, 14, BLACK);
    
    // Display temperature warnings
    if (data.temperature > 32){
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 50);
      display.println("TEMP HIGH");
      warning = true;
    }
    else if (data.temperature < 24){
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 50);
      display.println("TEMP LOW");
      warning = true;
    }
    
    // Display humidity warnings
    if (data.humidity > 80){
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor((data.temperature > 32 || data.temperature < 24) ? 70 : 0, 50);
      display.println("HUM HIGH");
      warning = true;
    }
    else if (data.humidity < 65){
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor((data.temperature > 32 || data.temperature < 24) ? 70 : 0, 50);
      display.println("HUM LOW");
      warning = true;
    }
    
    // Only refresh display if there's a warning
    if (warning) {
      display.display();
    }
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

        //save to EEPROM
        save_settings();

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

void view_alarms() {
  display.clearDisplay();
  
  if (!alarm_enabled) {
    print_line("Alarms disabled", 0, 0, 2);
    delay(2000);
    return;
  }
  
  for (int i = 0; i < n_alarms; i++) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Alarm ");
    display.print(i+1);
    display.print(":");
    
    display.setCursor(0, 20);
    // Format time with leading zeros
    if (alarm_hours[i] < 10) display.print("0");
    display.print(alarm_hours[i]);
    display.print(":");
    if (alarm_minutes[i] < 10) display.print("0");
    display.print(alarm_minutes[i]);
    
    display.setCursor(0, 40);
    display.setTextSize(1);
    display.print("Status: ");
    display.print(alarm_triggered[i] ? "Triggered" : "Waiting");
    display.setCursor(0, 50);
    display.print("Repeat: ");
    display.print(alarm_repeat[i] ? "Yes" : "No");

    
    display.display();
    
    // Wait for button press to see next alarm or exit
    bool exitLoop = false;
    while (!exitLoop) {
      if (digitalRead(PB_OK) == LOW) {
        delay(200);
        exitLoop = true; // Go to next alarm
      }
      else if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        return; // Exit to main menu
      }
    }
  }
}

void delete_alarm() {
  int current_alarm = 0;
  
  while (true) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Alarm ");
    display.print(current_alarm + 1);
    
    display.setCursor(0, 20);
    if (alarm_hours[current_alarm] < 10) display.print("0");
    display.print(alarm_hours[current_alarm]);
    display.print(":");
    if (alarm_minutes[current_alarm] < 10) display.print("0");
    display.print(alarm_minutes[current_alarm]);
    
    display.setCursor(0, 45);
    display.setTextSize(1);
    display.print("UP/DOWN=Select OK=Delete");
    
    display.display();
    
    int pressed = wait_for_button_press();
    
    if (pressed == PB_UP || pressed == PB_DOWN) {
      current_alarm = (current_alarm + 1) % n_alarms;
    }
    else if (pressed == PB_OK) {
      // Reset this alarm
      alarm_hours[current_alarm] = 0;
      alarm_minutes[current_alarm] = 0;
      alarm_triggered[current_alarm] = false;

      save_settings();
      
      display.clearDisplay();
      print_line("Alarm deleted", 0, 0, 2);
      delay(1500);
      return;
    }
    else if (pressed == PB_CANCEL) {
      return;
    }
  }
}

void save_settings() {
  prefs.begin("medibox", false);  // false = write mode

  prefs.putFloat("tz_offset", UTC_OFFSET);
  prefs.putBool("alarm_en", alarm_enabled);
  for (int i = 0; i < n_alarms; i++) {
    prefs.putInt(("a_hr" + String(i)).c_str(), alarm_hours[i]);
    prefs.putInt(("a_min" + String(i)).c_str(), alarm_minutes[i]);
    prefs.putBool(("a_rep" + String(i)).c_str(), alarm_repeat[i]);
  }

  prefs.end();
}



