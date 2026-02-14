#include <LiquidCrystal.h>
#include <TM1637Display.h>

/* =========================
   하드웨어 설정
========================= */
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

const int RESET_BTN = 2;
const int MODE_BTN  = 3;
const int GREEN_LED = 6;
const int RED_LED   = 13;

/* =========================
   상태 정의
========================= */
enum ExerciseMode { MODE_SELECT, PUSHUP };
enum SystemState { PREPARE, BASELINE_CAPTURE, CALIBRATION, NORMAL };

ExerciseMode currentExercise = MODE_SELECT;
SystemState systemState = PREPARE;

/* =========================
   전역 변수
========================= */
int selectedIndex = 0;
const char* modeNames[] = {"PUSH-UP"};

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

/* ===== 운동 ===== */
int pushUpCount = 0;
int pushState = 0; // 0=READY(위), 1=DOWN(아래)

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

  if (digitalRead(RESET_BTN) == LOW) {
    delay(300);
    resetSystem();
    return;
  }

  handlePushUp();
}

/* =========================
   MODE SELECT
========================= */
void handleModeSelect() {

  lcd.setCursor(0,0);
  lcd.print("SELECT MODE   ");
  lcd.setCursor(0,1);
  lcd.print("> ");
  lcd.print(modeNames[selectedIndex]);
  lcd.print("        ");

  if (digitalRead(MODE_BTN) == LOW) {
    delay(250);
    selectedIndex = (selectedIndex + 1) % 1;
  }

  if (digitalRead(RESET_BTN) == LOW) {
    delay(300);
    currentExercise = PUSHUP;
    systemState = PREPARE;
    stateStartTime = millis();
    lcd.clear();
  }
}

/* =========================
   PUSHUP 통합 로직
========================= */
void handlePushUp() {

  int leftVal  = analogRead(A0);
  int rightVal = analogRead(A1);
  int currentSum = leftVal + rightVal;

  /* =========================
     1단계: PREPARE (5초 카운트)
  ========================= */
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

  /* =========================
     2단계: BASELINE_CAPTURE (3초 평균 측정)
  ========================= */
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
      stateStartTime = millis();
      lcd.clear();
      lcd.print("Calibrate 1/3  ");
      delay(800);
      lcd.clear();
    }
    return;
  }

  /* =========================
     3단계: CALIBRATION (3회 깊이 학습)
  ========================= */
  if (systemState == CALIBRATION) {

    lcd.setCursor(0,0);
    lcd.print("Calibrate ");
    lcd.print(caliCount+1);
    lcd.print("/3     ");

    lcd.setCursor(0,1);
    lcd.print("SUM:");
    lcd.print(currentSum);
    lcd.print("     ");

    float threshold = (downAvg == 0) ? baseSum * 0.15 : (downAvg - baseSum) * 0.15;
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

  /* =========================
     4단계: NORMAL (상대 위치 기반)
  ========================= */

  float range = downAvg - baseSum;

  float downThreshold = baseSum + range * 0.75; // 75% 이상 내려가야 DOWN
  float upThreshold   = baseSum + range * 0.35; // 35% 이하 올라오면 UP 인정

  bool isBalanced = abs(leftVal - rightVal) < (currentSum * 0.4);

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

  // 내려감 감지
  if (pushState == 0 && currentSum >= downThreshold) {

    if (isBalanced) {
      pushState = 1;
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
    } else {
      digitalWrite(RED_LED, HIGH);
    }
  }

  // 올라옴 감지 → 카운트 증가
  else if (pushState == 1 && currentSum <= upThreshold) {
    pushState = 0;
    pushUpCount++;
    display.showNumberDec(pushUpCount);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
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
