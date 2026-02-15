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
   Mode Select
========================= */
int selectedIndex = 0;
const int TOTAL_MODES = 3;
const char* modeNames[TOTAL_MODES] = {
  "PUSH-UP",
  "SQUAT",
  "PLANK"
};

unsigned long stateStartTime = 0;

/* ===== Shared Calibration Vars (Push-up용) ===== */
float baseSum = 0;
float baseAccumulator = 0;
int baseSamples = 0;

const int CALI_TARGET = 3;
int caliCount = 0;
float downPeaks[3];
float downAvg = 0;
float currentPeak = 0;

int pushUpCount = 0;
int pushState = 0;

/* ===== Squat Vars ===== */
float squatBase = 0;
float squatAccumulator = 0;
int squatSamples = 0;
int squatCaliCount = 0;
float squatPeaks[3];
float squatDownAvg = 0;
float squatCurrentPeak = 0;
int squatState = 0;
int squatCount = 0;

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
      resetPushup();
      return;
    }
    handlePushUp();
  }

  else if (currentExercise == SQUAT) {
    if (digitalRead(RESET_BTN) == LOW) {
      delay(300);
      resetSquat();
      return;
    }
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

    currentExercise = (ExerciseMode)(selectedIndex + 1);
    systemState = PREPARE;
    stateStartTime = millis();
    lcd.clear();
  }
}

/* ==================================================
   PUSH-UP (완전 보존)
================================================== */
void handlePushUp() {

  int leftVal  = analogRead(A0);
  int rightVal = analogRead(A1);
  int currentSum = leftVal + rightVal;

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

/* ==================================================
   SQUAT (Push-up과 동일 구조)
================================================== */
void handleSquat() {

  int currentSum = analogRead(A0) + analogRead(A1);

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
      squatAccumulator = 0;
      squatSamples = 0;
      lcd.clear();
    }
    return;
  }

  if (systemState == BASELINE_CAPTURE) {

    unsigned long elapsed = millis() - stateStartTime;

    squatAccumulator += currentSum;
    squatSamples++;

    lcd.setCursor(0,0);
    lcd.print("Stand Still    ");
    lcd.setCursor(0,1);
    lcd.print("Measuring...   ");

    if (elapsed >= 3000) {
      squatBase = squatAccumulator / squatSamples;
      systemState = CALIBRATION;
      squatCaliCount = 0;
      lcd.clear();
    }
    return;
  }

  if (systemState == CALIBRATION) {

    float threshold = squatBase * 0.20;

    lcd.setCursor(0,0);
    lcd.print("Calibrate ");
    lcd.print(squatCaliCount+1);
    lcd.print("/3     ");

    if (squatState == 0 && (currentSum - squatBase) > threshold) {
      squatState = 1;
      squatCurrentPeak = currentSum;
    }

    if (squatState == 1) {
      if (currentSum > squatCurrentPeak)
        squatCurrentPeak = currentSum;

      if ((currentSum - squatBase) < threshold * 0.5) {
        squatPeaks[squatCaliCount] = squatCurrentPeak;
        squatCaliCount++;
        squatState = 0;
        delay(400);
      }
    }

    if (squatCaliCount >= 3) {
      squatDownAvg = (squatPeaks[0] + squatPeaks[1] + squatPeaks[2]) / 3.0;
      systemState = NORMAL;
      lcd.clear();
      lcd.print("START!         ");
      delay(1000);
      lcd.clear();
    }
    return;
  }

  float range = squatDownAvg - squatBase;
  float downThreshold = squatBase + range * 0.75;
  float upThreshold   = squatBase + range * 0.35;

  lcd.setCursor(0,0);
  lcd.print("Squat Count    ");
  lcd.setCursor(0,1);
  lcd.print(squatCount);
  lcd.print(" reps          ");

  if (squatState == 0 && currentSum >= downThreshold) {
    squatState = 1;
  }
  else if (squatState == 1 && currentSum <= upThreshold) {
    squatState = 0;
    squatCount++;
    display.showNumberDec(squatCount);
    delay(250);
  }
}

/* =========================
   PLANK (문구만)
========================= */
void handlePlank() {

  lcd.setCursor(0,0);
  lcd.print("PLANK MODE     ");
  lcd.setCursor(0,1);
  lcd.print("Plank Running  ");
}

/* =========================
   RESET
========================= */
void resetPushup() {
  systemState = PREPARE;
  pushUpCount = 0;
  pushState = 0;
  caliCount = 0;
  baseSamples = 0;
  baseAccumulator = 0;
  baseSum = 0;
  downAvg = 0;
  lcd.clear();
}

void resetSquat() {
  systemState = PREPARE;
  squatState = 0;
  squatCount = 0;
  squatSamples = 0;
  squatAccumulator = 0;
  squatCaliCount = 0;
  lcd.clear();
}
