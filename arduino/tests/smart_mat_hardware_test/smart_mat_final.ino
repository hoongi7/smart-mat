#include <LiquidCrystal.h>
#include <TM1637Display.h>

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

const int RESET_BTN = 2;
const int MODE_BTN  = 3;
const int GREEN_LED = 6;
const int RED_LED   = 13;

const int S_FL = A0;
const int S_FR = A1;
const int S_RL = A2;
const int S_RR = A3;

enum ExerciseMode { MODE_SELECT, PUSHUP, SQUAT, PLANK };
enum SystemState  { PREPARE, BASELINE_CAPTURE, CALIBRATION, NORMAL };

ExerciseMode currentExercise = MODE_SELECT;
SystemState  systemState     = PREPARE;

int selectedIndex = 0;
const int TOTAL_MODES = 3;
const char* modeNames[TOTAL_MODES] = { "PUSH-UP", "SQUAT", "PLANK" };

unsigned long stateStartTime = 0;
unsigned long lastLcdUpdate  = 0;
const unsigned long LCD_INTERVAL = 250;

float baseVal      = 0;
float baseRatio    = 0;
float caliSumAcc   = 0;
float caliRatioAcc = 0;
long  sampleCount  = 0;

float caliPeaks[3];
int   caliStep       = 0;
float activeTh       = 0;
int   exerciseCount  = 0;
int   motionState    = 0;
float pu_currentPeak = 0;

bool          isPlanking = false;
unsigned long plankStart = 0;

/* =========================
   SETUP
========================= */
void setup() {
  lcd.begin(16, 2);
  display.setBrightness(0x0f);
  display.showNumberDec(0);
  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(MODE_BTN,  INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  lcd.print("SMART MAT V5.0");
  delay(1500);
  lcd.clear();
}

/* =========================
   MAIN LOOP
========================= */
void loop() {
  if (currentExercise == MODE_SELECT) {
    handleModeSelect();
    return;
  }
  checkResetLogic();
  switch (systemState) {
    case PREPARE:          handlePrepare();      break;
    case BASELINE_CAPTURE: handleBaseline();     break;
    case CALIBRATION:      handleCalibration();  break;
    case NORMAL:           handleNormal();       break;
  }
}

/* =========================
   MODE SELECT
========================= */
void handleModeSelect() {
  lcd.setCursor(0, 0); lcd.print("SELECT MODE     ");
  lcd.setCursor(0, 1); lcd.print("> "); lcd.print(modeNames[selectedIndex]);
  lcd.print("            ");

  if (digitalRead(MODE_BTN) == LOW) {
    delay(250);
    selectedIndex = (selectedIndex + 1) % TOTAL_MODES;
  }
  if (digitalRead(RESET_BTN) == LOW) {
    delay(300);
    currentExercise = (ExerciseMode)(selectedIndex + 1);
    systemState     = PREPARE;
    stateStartTime  = millis();
    caliSumAcc = 0; caliRatioAcc = 0; sampleCount = 0;
    lcd.clear();
  }
}

/* =========================
   PREPARE (5초)
========================= */
void handlePrepare() {
  unsigned long elapsed = millis() - stateStartTime;
  int remain = 5 - (int)(elapsed / 1000);
  if (remain < 0) remain = 0;

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    if (currentExercise == PLANK) {
      lcd.setCursor(0, 0); lcd.print("Get into PLANK  ");
      lcd.setCursor(0, 1); lcd.print("Starting in "); lcd.print(remain); lcd.print("s  ");
    } else {
      lcd.setCursor(0, 0); lcd.print("Ready? "); lcd.print(modeNames[currentExercise - 1]);
      lcd.setCursor(0, 1); lcd.print("Wait... "); lcd.print(remain); lcd.print("s   ");
    }
  }

  if (elapsed >= 5000) {
    systemState    = BASELINE_CAPTURE;
    stateStartTime = millis();
    caliSumAcc = 0; caliRatioAcc = 0; sampleCount = 0;
    lcd.clear();
  }
}

/* =========================
   BASELINE (3초 측정)
   푸쉬업: 손 2센서 → baseVal
   스쿼트: 뒤꿈치 비중 → baseRatio
   플랭크: 4센서 → baseVal, 완료 즉시 타이머 시작 → NORMAL
========================= */
void handleBaseline() {
  unsigned long elapsed = millis() - stateStartTime;
  int remain = 3 - (int)(elapsed / 1000);
  if (remain < 0) remain = 0;

  if (currentExercise == SQUAT) {
    caliRatioAcc += getSquatRatio();
  } else if (currentExercise == PLANK) {
    caliSumAcc += (analogRead(S_FL) + analogRead(S_FR) + analogRead(S_RL) + analogRead(S_RR));
  } else {
    caliSumAcc += (analogRead(S_FL) + analogRead(S_FR));
  }
  sampleCount++;

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    if (currentExercise == PLANK) {
      lcd.setCursor(0, 0); lcd.print("Hold PLANK!     ");
      lcd.setCursor(0, 1); lcd.print("Measuring.. "); lcd.print(remain); lcd.print("s  ");
    } else {
      lcd.setCursor(0, 0); lcd.print("Base (3s)       ");
      lcd.setCursor(0, 1); lcd.print("Stay Still.. "); lcd.print(remain); lcd.print("s  ");
    }
  }

  if (elapsed >= 3000) {
    if (currentExercise == SQUAT) {
      baseRatio = caliRatioAcc / (float)sampleCount;
    } else {
      baseVal = caliSumAcc / (float)sampleCount;
    }

    if (currentExercise == PLANK) {
      isPlanking  = true;
      plankStart  = millis();
      systemState = NORMAL;
      lcd.clear();
      lcd.print("PLANK START!");
      delay(500);
      lcd.clear();
    } else {
      systemState    = CALIBRATION;
      caliStep       = 0;
      motionState    = 0;
      pu_currentPeak = 0;
      lcd.clear();
    }
  }
}

/* =========================
   CALIBRATION (푸쉬업/스쿼트만)
========================= */
void handleCalibration() {
  float current   = (currentExercise == SQUAT) ? getSquatRatio()
                                               : (analogRead(S_FL) + analogRead(S_FR));
  float reference = (currentExercise == SQUAT) ? baseRatio : baseVal;
  float diff      = current - reference;
  float trigger   = (currentExercise == SQUAT) ? 0.12 : 50.0;

  if (motionState == 0 && diff > trigger) {
    motionState    = 1;
    pu_currentPeak = current;
  } else if (motionState == 1) {
    if (current > pu_currentPeak) pu_currentPeak = current;
    if (diff < trigger * 0.5) {
      caliPeaks[caliStep] = pu_currentPeak;
      caliStep++;
      motionState    = 0;
      pu_currentPeak = 0;
      delay(500);
    }
  }

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0, 0); lcd.print("Cali "); lcd.print(caliStep + 1); lcd.print("/3      ");
    lcd.setCursor(0, 1); lcd.print("Do 1 Rep!       ");
  }

  if (caliStep >= 3) {
    activeTh      = (caliPeaks[0] + caliPeaks[1] + caliPeaks[2]) / 3.0;
    systemState   = NORMAL;
    exerciseCount = 0;
    display.showNumberDec(0);
    lcd.clear();
    lcd.print("SYSTEM READY!");
    delay(1000);
    lcd.clear();
  }
}

/* =========================
   NORMAL
========================= */
void handleNormal() {
  if      (currentExercise == PUSHUP) handleNormalPushUp();
  else if (currentExercise == SQUAT)  handleNormalSquat();
  else if (currentExercise == PLANK)  handleNormalPlank();
}

/* ── 푸쉬업 (원본 그대로) ── */
void handleNormalPushUp() {
  int leftVal   = analogRead(S_FL);
  int rightVal  = analogRead(S_FR);
  float current = leftVal + rightVal;

  float range  = activeTh - baseVal;
  if (range < 40) range = 40;
  float downTh = baseVal + range * 0.75;
  float upTh   = baseVal + range * 0.35;

  if (motionState == 0 && current >= downTh) {
    motionState = 1;
    digitalWrite(GREEN_LED, HIGH);
  } else if (motionState == 1 && current <= upTh) {
    motionState = 0;
    exerciseCount++;
    display.showNumberDec(exerciseCount);
    digitalWrite(GREEN_LED, LOW);
    delay(250);
  }

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0, 0);
    lcd.print("L:"); lcd.print(leftVal); lcd.print(" R:"); lcd.print(rightVal); lcd.print("   ");
    lcd.setCursor(0, 1);
    lcd.print("CNT:"); lcd.print(exerciseCount); lcd.print("     ");
  }
}

/* ── 스쿼트 (원본 그대로) ── */
void handleNormalSquat() {
  float current = getSquatRatio();

  float range  = activeTh - baseRatio;
  if (range < 0.05) range = 0.05;
  float downTh = baseRatio + range * 0.75;
  float upTh   = baseRatio + range * 0.35;

  float fL    = analogRead(S_FL);
  float fR    = analogRead(S_FR);
  float rL    = analogRead(S_RL);
  float rR    = analogRead(S_RR);
  float total = fL + fR + rL + rR;
  float lrRatio = (total > 10) ? ((fL + rL) / total) : 0.5;
  bool  leaning = (lrRatio > 0.65 || lrRatio < 0.35);

  if (motionState == 0 && current >= downTh) {
    motionState = 1;
    digitalWrite(GREEN_LED, HIGH);
  } else if (motionState == 1 && current <= upTh) {
    motionState = 0;
    exerciseCount++;
    display.showNumberDec(exerciseCount);
    digitalWrite(GREEN_LED, LOW);
    delay(300);
  }

  if (leaning && motionState == 1) digitalWrite(RED_LED, HIGH);
  else                              digitalWrite(RED_LED, LOW);

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0, 0);
    lcd.print("R:"); lcd.print(current, 2);
    lcd.print(" B:"); lcd.print(baseRatio, 2);
    lcd.print("   ");
    lcd.setCursor(0, 1);
    lcd.print("CNT:"); lcd.print(exerciseCount);
    if (leaning && motionState == 1) { lcd.print(" !LEAN!"); }
    else                             { lcd.print("       "); }
  }
}

/* ── 플랭크 ──
   baseVal: 엎드린 상태 3초 측정값
   baseVal*0.7 이상 → PLANKING + 타이머
   baseVal*0.7 미만 → REST
*/
void handleNormalPlank() {
  float current    = analogRead(S_FL) + analogRead(S_FR) + analogRead(S_RL) + analogRead(S_RR);
  float holdThresh = baseVal * 0.7;

  if (current >= holdThresh) {
    if (!isPlanking) {
      isPlanking = true;
      plankStart = millis();
      lcd.clear();
    }
    long elapsedSec = (millis() - plankStart) / 1000;
    display.showNumberDec(elapsedSec);
    digitalWrite(GREEN_LED, HIGH);
    if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
      lastLcdUpdate = millis();
      lcd.setCursor(0, 0); lcd.print("PLANKING!       ");
      lcd.setCursor(0, 1); lcd.print("TIME: "); lcd.print(elapsedSec); lcd.print("s   ");
    }
  } else {
    if (isPlanking) {
      isPlanking = false;
      digitalWrite(GREEN_LED, LOW);
      lcd.clear();
    }
    if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
      lastLcdUpdate = millis();
      lcd.setCursor(0, 0); lcd.print("REST / FALLEN   ");
      lcd.setCursor(0, 1);
      lcd.print("V:"); lcd.print((int)current);
      lcd.print(" T:"); lcd.print((int)holdThresh);
      lcd.print("    ");
    }
  }
}

/* =========================
   스쿼트 비중 계산
========================= */
float getSquatRatio() {
  float front = analogRead(S_FL) + analogRead(S_FR);
  float rear  = analogRead(S_RL) + analogRead(S_RR);
  float total = front + rear;
  if (total < 20) return 0;
  return (rear / total);
}

/* =========================
   리셋
   RESET 단독       → 현재 모드 PREPARE부터 재시작
   RESET + MODE 동시 → 모드 선택으로 완전 초기화
========================= */
void checkResetLogic() {
  bool resetDown = (digitalRead(RESET_BTN) == LOW);
  bool modeDown  = (digitalRead(MODE_BTN)  == LOW);

  if (resetDown && modeDown) {
    delay(500);
    resetAll();
  } else if (resetDown) {
    delay(300);
    resetExercise();
  }
}

void resetExercise() {
  systemState    = PREPARE;
  stateStartTime = millis();
  exerciseCount  = 0;
  motionState    = 0;
  isPlanking     = false;
  pu_currentPeak = 0;
  caliStep       = 0;
  baseVal        = 0;
  baseRatio      = 0;
  activeTh       = 0;
  caliSumAcc     = 0;
  caliRatioAcc   = 0;
  sampleCount    = 0;
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED,   LOW);
  display.showNumberDec(0);
  lcd.clear();
  lcd.print("RECALIBRATING");
  delay(800);
  lcd.clear();
}

void resetAll() {
  currentExercise = MODE_SELECT;
  selectedIndex   = 0;
  systemState     = PREPARE;
  exerciseCount   = 0;
  motionState     = 0;
  isPlanking      = false;
  pu_currentPeak  = 0;
  caliStep        = 0;
  baseVal         = 0;
  baseRatio       = 0;
  activeTh        = 0;
  caliSumAcc      = 0;
  caliRatioAcc    = 0;
  sampleCount     = 0;
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED,   LOW);
  display.showNumberDec(0);
  lcd.clear();
}
