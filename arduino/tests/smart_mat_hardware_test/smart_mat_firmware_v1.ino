#include <LiquidCrystal.h>
#include <TM1637Display.h>

/* =========================
   Hardware
========================= */
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

const int RESET_BTN = 2;
const int MODE_BTN  = 3;
const int GREEN_LED = 6;
const int RED_LED   = 13;

/* =========================
   State
========================= */
enum ExerciseMode { MODE_SELECT, PUSHUP, SQUAT, PLANK };
enum SystemState { PREPARE, BASELINE_CAPTURE, CALIBRATION, NORMAL };

ExerciseMode currentExercise = MODE_SELECT;
SystemState systemState = PREPARE;

/* =========================
   Global
========================= */
int selectedIndex = 0;
const int TOTAL_MODES = 3;
const char* modeNames[TOTAL_MODES] = {
  "PUSH-UP",
  "SQUAT",
  "PLANK"
};

unsigned long stateStartTime = 0;

/* ===== baseline ===== */
float baseSum = 0;
float baseAccumulator = 0;
int baseSamples = 0;

/* ===== calibration ===== */
const int CALI_TARGET = 3;
int caliCount = 0;
float downPeaks[3];
float downAvg = 0;
float currentPeak = 0;

/* ===== push-up ===== */
int pushUpCount = 0;
int pushState = 0; // 0=UP, 1=DOWN

/* =========================
   SETUP
========================= */
void setup() {
  lcd.begin(16, 2);
  display.setBrightness(0x0f);

  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  display.showNumberDec(0);

  lcd.print("SMART MAT");
  delay(1000);
  lcd.clear();
}

/* =========================
   LOOP
========================= */
void loop() {

  if (currentExercise == MODE_SELECT) {
    handleModeSelect();
    return;
  }

  if (currentExercise == PUSHUP) {
    if (digitalRead(RESET_BTN) == LOW) {
      delay(300);
      resetSystem();
      return;
    }
    handlePushUp();
  }

  else if (currentExercise == SQUAT) {
    handleSquat();
  }

  else if (currentExercise == PLANK) {
    handlePlank();
  }
}

/* =========================
   MODE SELECT
========================= */
void handleModeSelect() {

  lcd.setCursor(0,0);
  lcd.print("SELECT MODE    ");

  lcd.setCursor(0,1);
  lcd.print("> ");
  lcd.print(modeNames[selectedIndex]);
  lcd.print("        ");

  if (digitalRead(MODE_BTN) == LOW) {
    delay(250);
    selectedIndex = (selectedIndex + 1) % TOTAL_MODES;
  }

  if (digitalRead(RESET_BTN) == LOW) {
    delay(300);

    if (selectedIndex == 0) currentExercise = PUSHUP;
    else if (selectedIndex == 1) currentExercise = SQUAT;
    else if (selectedIndex == 2) currentExercise = PLANK;

    systemState = PREPARE;
    stateStartTime = millis();
    lcd.clear();
  }
}

/* =========================
   SQUAT
========================= */
void handleSquat() {

  lcd.setCursor(0,0);
  lcd.print("SQUAT MODE     ");

  lcd.setCursor(0,1);
  lcd.print("Squat Running  ");

  display.showNumberDec(0);

  if (digitalRead(RESET_BTN) == LOW) {
    delay(300);
    currentExercise = MODE_SELECT;
    lcd.clear();
  }
}

/* =========================
   PLANK
========================= */
void handlePlank() {

  lcd.setCursor(0,0);
  lcd.print("PLANK MODE     ");

  lcd.setCursor(0,1);
  lcd.print("Plank Running  ");

  display.showNumberDec(0);

  if (digitalRead(RESET_BTN) == LOW) {
    delay(300);
    currentExercise = MODE_SELECT;
    lcd.clear();
  }
}

/* =========================
   PUSHUP
========================= */
void handlePushUp() {

  int leftVal  = analogRead(A0);
  int rightVal = analogRead(A1);
  int currentSum = leftVal + rightVal;

  /* PREPARE 5s */
  if (systemState == PREPARE) {

    unsigned long elapsed = millis() - stateStartTime;
    int remain = 5 - (elapsed / 1000);
    if (remain < 0) remain = 0;

    lcd.setCursor(0,0);
    lcd.print("Prepare...     ");
    lcd.setCursor(0,1);
    lcd.print("Start in ");
    lcd.print(remain);
    lcd.print("s      ");

    if (elapsed >= 5000) {
      systemState = BASELINE_CAPTURE;
      stateStartTime = millis();
      baseAccumulator = 0;
      baseSamples = 0;
      lcd.clear();
    }
    return;
  }

  /* BASELINE 3s */
  if (systemState == BASELINE_CAPTURE) {

    unsigned long elapsed = millis() - stateStartTime;
    int remain = 3 - (elapsed / 1000);
    if (remain < 0) remain = 0;

    baseAccumulator += currentSum;
    baseSamples++;

    lcd.setCursor(0,0);
    lcd.print("Hold Position  ");
    lcd.setCursor(0,1);
    lcd.print("Measure ");
    lcd.print(remain);
    lcd.print("s      ");

    if (elapsed >= 3000) {
      baseSum = baseAccumulator / baseSamples;
      systemState = CALIBRATION;
      lcd.clear();
      lcd.print("Calibrate 1/3  ");
      delay(800);
      lcd.clear();
    }
    return;
  }

  /* CALIBRATION */
  if (systemState == CALIBRATION) {

    lcd.setCursor(0,0);
    lcd.print("Calibrate ");
    lcd.print(caliCount+1);
    lcd.print("/3     ");

    lcd.setCursor(0,1);
    lcd.print("SUM:");
    lcd.print(currentSum);
    lcd.print("     ");

    float threshold = baseSum * 0.15;
    if (threshold < 40) threshold = 40;

    if (pushState == 0 && (currentSum - baseSum) > threshold) {
      pushState = 1;
      currentPeak = currentSum;
    }

    if (pushState == 1) {
      if (currentSum > currentPeak)
        currentPeak = currentSum;

      if ((currentSum - baseSum) < threshold * 0.5) {
        downPeaks[caliCount] = currentPeak;
        caliCount++;
        pushState = 0;
        currentPeak = 0;
        delay(400);
      }
    }

    if (caliCount >= CALI_TARGET) {
      downAvg = (downPeaks[0] + downPeaks[1] + downPeaks[2]) / 3.0;
      systemState = NORMAL;
      lcd.clear();
      lcd.print("START!         ");
      delay(1000);
      lcd.clear();
    }
    return;
  }

  /* NORMAL */
  float range = downAvg - baseSum;
  float downThreshold = baseSum + range * 0.75;
  float upThreshold   = baseSum + range * 0.35;

  lcd.setCursor(0,0);
  lcd.print("L:");
  lcd.print(leftVal);
  lcd.print(" R:");
  lcd.print(rightVal);
  lcd.print("   ");

  lcd.setCursor(0,1);
  lcd.print("CNT:");
  lcd.print(pushUpCount);
  lcd.print("     ");

  if (pushState == 0 && currentSum >= downThreshold) {
    pushState = 1;
    digitalWrite(GREEN_LED, HIGH);
  }

  else if (pushState == 1 && currentSum <= upThreshold) {
    pushState = 0;
    pushUpCount++;
    display.showNumberDec(pushUpCount);
    digitalWrite(GREEN_LED, LOW);
    delay(250);
  }
}

/* =========================
   RESET
========================= */
void resetSystem() {

  currentExercise = MODE_SELECT;
  systemState = PREPARE;

  pushUpCount = 0;
  pushState = 0;
  caliCount = 0;
  baseSamples = 0;
  baseAccumulator = 0;
  baseSum = 0;
  downAvg = 0;

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  display.showNumberDec(0);
  lcd.clear();
}
