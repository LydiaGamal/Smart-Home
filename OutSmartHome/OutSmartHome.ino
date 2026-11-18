#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

// ==================================================
// 1. KEYPAD
// ==================================================

const byte ROW_PINS[4] = {A0, A1, A2, A3};
const byte COL_PINS[4] = {9, 8, 7, 6};

char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

Keypad keypad = Keypad(
  makeKeymap(keys),
  (byte*)ROW_PINS,
  (byte*)COL_PINS,
  4,
  4
);

// ==================================================
// 2. PIN CONFIGURATION
// ==================================================

const byte BUTTON_PIN = 2;
const byte BUZZER_PIN = 3;
const byte SERVO_PIN = 4;
const byte LED_PIN = 12; 
const byte TRIG_PIN = 10;
const byte ECHO_PIN = 11;

// ==================================================
// 3. SETTINGS
// ==================================================

const char PASSWORD[] = "1234";

const int CLOSED_ANGLE = 0;
const int OPEN_ANGLE = 90;

const unsigned long DOOR_OPEN_TIME = 5000;
const unsigned long BUTTON_DEBOUNCE = 40;
const unsigned long MESSAGE_TIME = 2000;
const unsigned long DISTANCE_INTERVAL = 100;

const float DISTANCE_LIMIT = 20.0;

// ==================================================
// 4. OBJECTS
// ==================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorServo;

// ==================================================
// 5. VARIABLES
// ==================================================

char enteredPassword[5];
byte passwordLength = 0;

bool doorOpen = false;
unsigned long doorOpenedAt = 0;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastButtonChange = 0;

float distanceCM = -1.0;
bool objectNearby = false;

unsigned long lastDistanceRead = 0;

bool messageActive = false;
char lcdMessage[17] = "ENTER PASSWORD";
unsigned long messageUntil = 0;

bool lcdVisible = false;
char lastLine1[17] = "";
char lastLine2[17] = "";

// ==================================================
// 6. LCD FUNCTIONS
// ==================================================

void updateLCD(const char* line1, const char* line2) {

  // LCD stays completely OFF when no object is nearby.
  if (!objectNearby) {

    if (lcdVisible) {
      lcd.clear();
      lcd.noBacklight();

      lcdVisible = false;

      lastLine1[0] = '\0';
      lastLine2[0] = '\0';
    }

    return;
  }

  // Turn LCD ON when an object is detected.
  lcd.backlight();

  // Update only when the displayed text changes.
  if (!lcdVisible ||
      strcmp(lastLine1, line1) != 0 ||
      strcmp(lastLine2, line2) != 0) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(line1);

    lcd.setCursor(0, 1);
    lcd.print(line2);

    strncpy(lastLine1, line1, 16);
    lastLine1[16] = '\0';

    strncpy(lastLine2, line2, 16);
    lastLine2[16] = '\0';

    lcdVisible = true;
  }
}

void showPassword() {
  updateLCD("ENTER PASSWORD", enteredPassword);
}

void showLCDMessage(const char* message) {
  strncpy(lcdMessage, message, 16);
  lcdMessage[16] = '\0';

  updateLCD(lcdMessage, "");
}

void showTemporaryMessage(const char* message) {
  messageActive = true;
  messageUntil = millis() + MESSAGE_TIME;

  showLCDMessage(message);
}

void refreshLCD() {

  if (!objectNearby) {
    updateLCD("", "");
    return;
  }

  if (messageActive) {

    if ((long)(millis() - messageUntil) < 0) {
      showLCDMessage(lcdMessage);
      return;
    }

    messageActive = false;
  }

  showPassword();
}

// ==================================================
// 7. DOOR CONTROL
// ==================================================

void openDoor() {
  doorServo.write(OPEN_ANGLE);

  doorOpen = true;
  doorOpenedAt = millis();

  Serial.println("Door opened");
}

void closeDoor() {
  doorServo.write(CLOSED_ANGLE);
  doorOpen = false;

  Serial.println("Door closed");
}

void handleDoorTimer() {
  if (doorOpen &&
      millis() - doorOpenedAt >= DOOR_OPEN_TIME) {
    closeDoor();
  }
}

// ==================================================
// 8. PASSWORD FUNCTIONS
// ==================================================

void clearPassword() {
  passwordLength = 0;
  enteredPassword[0] = '\0';
}

void submitPassword() {
  enteredPassword[passwordLength] = '\0';

  if (strcmp(enteredPassword, PASSWORD) == 0) {

    Serial.println("Correct password");

    clearPassword();

    showTemporaryMessage("CORRECT PASSWORD");
    openDoor();

  } else {

    Serial.println("Wrong password");

    clearPassword();

    showTemporaryMessage("WRONG PASSWORD");

    tone(BUZZER_PIN, 1000, 500);
  }
}

void handleKeypad() {

  if (messageActive) {

    if ((long)(millis() - messageUntil) < 0) {
      return;
    }

    messageActive = false;
    showPassword();
  }

  char key = keypad.getKey();

  if (!key) {
    return;
  }

  Serial.print("Key pressed: ");
  Serial.println(key);

  if (key >= '0' && key <= '9') {

    if (passwordLength < 4) {

      enteredPassword[passwordLength] = key;
      passwordLength++;

      enteredPassword[passwordLength] = '\0';

      showPassword();
    }
  }

  else if (key == '*') {

    clearPassword();
    showPassword();
  }

  else if (key == '#') {

    if (passwordLength > 0) {
      submitPassword();
    }
  }
}

// ==================================================
// 9. SWITCH
// ==================================================

void handleButton() {

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonChange = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastButtonChange >= BUTTON_DEBOUNCE) {

    if (reading != stableButtonState) {

      stableButtonState = reading;

      Serial.println("Switch changed");
      openDoor();
    }
  }
}

// ==================================================
// 10. ULTRASONIC SENSOR AND LED
// ==================================================

void handleUltrasonic() {

  if (millis() - lastDistanceRead < DISTANCE_INTERVAL) {
    return;
  }

  lastDistanceRead = millis();

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 25000);

  if (duration == 0) {
    distanceCM = -1.0;
  } else {
    distanceCM = duration * 0.0343 / 2.0;
  }

  objectNearby =
    (distanceCM > 0 && distanceCM < DISTANCE_LIMIT);

  // LED ON below 20 cm.
  digitalWrite(LED_PIN, objectNearby ? HIGH : LOW);

  // Update LCD according to object detection.
  refreshLCD();
}

// ==================================================
// 11. SETUP
// ==================================================

void setup() {

  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(ECHO_PIN, INPUT);

  doorServo.attach(SERVO_PIN);
  doorServo.write(CLOSED_ANGLE);

  Wire.begin();

  lcd.init();
  lcd.clear();
  lcd.noBacklight();

  clearPassword();

  Serial.println("System ready");
}

// ==================================================
// 12. MAIN LOOP
// ==================================================

void loop() {

  handleKeypad();

  handleButton();

  handleUltrasonic();

  handleDoorTimer();

  refreshLCD();
}