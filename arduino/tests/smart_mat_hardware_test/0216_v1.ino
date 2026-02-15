#include <LiquidCrystal.h>
#include <TM1637Display.h>

/* =========================
   Hardware & Pin Maps
========================= */
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

const int RESET_BTN = 2; 
const int MODE_BTN  = 3; 

const int S_FL = A0; // 앞 왼 (손/앞꿈치)
const int S_FR = A1; // 앞 오 (손/앞꿈치)
const int S_RL = A2; // 뒤 왼 (발/뒤꿈치)
const int S_RR = A3; // 뒤 오 (발/뒤꿈치)

/* =========================
   State & Global Variables
========================= */
enum ExerciseMode { MODE_SELECT, PUSHUP, SQUAT, PLANK };
enum SystemState { PREPARE, BASELINE_CAPTURE, CALIBRATION, NORMAL };

ExerciseMode currentExercise = MODE_SELECT;
SystemState systemState = PREPARE;

int selectedIndex = 0;
const int TOTAL_MODES = 3;
const char* modeNames[TOTAL_MODES] = { "PUSH-UP", "SQUAT", "PLANK" };

unsigned long stateStartTime = 0;
unsigned long lastLcdUpdate = 0;
const unsigned long LCD_INTERVAL = 250;

/* ==================================================
   COMMON VARIABLES
================================================== */
float baseVal = 0;       // 푸쉬업: 하중 합계 기준
float baseRatio = 0;     // 스쿼트: 뒤꿈치 비중 기준
float caliSumAcc = 0;
float caliRatioAcc = 0;
long  sampleCount = 0;

float caliPeaks[3];      // 3회 측정값
int   caliStep = 0;
float activeTh = 0; 

int   exerciseCount = 0;
int   motionState = 0;   // 0:UP, 1:DOWN

/* =========================
   SETUP
========================= */
void setup() {
  lcd.begin(16, 2);
  display.setBrightness(0x0f);
  display.showNumberDec(0);

  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(MODE_BTN, INPUT_PULLUP);

  lcd.print("SMART MAT V1.6");
  delay(1500);
  lcd.clear();
}

/* =========================
   CORE LOOP
========================= */
void loop() {
  if (currentExercise == MODE_SELECT) {
    handleModeSelect();
    return;
  }

  checkResetLogic();

  switch(systemState) {
    case PREPARE:          handlePrepare(); break;     
    case BASELINE_CAPTURE: handleBaseline(); break;    
    case CALIBRATION:      handleCalibration(); break; 
    case NORMAL:           handleNormal(); break;      
  }
}

/* =========================
   SQUAT 전용 비중 계산 로직
========================= */
float getSquatRatio() {
  float front = analogRead(S_FL) + analogRead(S_FR);
  float rear  = analogRead(S_RL) + analogRead(S_RR);
  float total = front + rear;
  
  if (total < 20) return 0; // 최소 하중 미달 시 0 반환
  return (rear / total);    // 전체 중 뒤꿈치 비중 (0.0 ~ 1.0)
}

/* =========================
   LOGIC HANDLERS
========================= */

void handleModeSelect() {
  lcd.setCursor(0,0);
  lcd.print("SELECT MODE     ");
  lcd.setCursor(0,1);
  lcd.print("> "); lcd.print(modeNames[selectedIndex]);
  lcd.print("            ");

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

void handlePrepare() {
  unsigned long elapsed = millis() - stateStartTime;
  int remain = 5 - (elapsed / 1000);

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0,0);
    lcd.print("Ready? "); lcd.print(modeNames[currentExercise-1]);
    lcd.setCursor(0,1);
    lcd.print("Wait... "); lcd.print(remain); lcd.print("s   ");
  }

  if (elapsed >= 5000) {
    systemState = BASELINE_CAPTURE;
    stateStartTime = millis();
    caliSumAcc = 0; caliRatioAcc = 0; sampleCount = 0;
    lcd.clear();
  }
}

void handleBaseline() {
  unsigned long elapsed = millis() - stateStartTime;
  int remain = 3 - (elapsed / 1000); 

  if (currentExercise == SQUAT) {
    caliRatioAcc += getSquatRatio(); // 스쿼트는 뒤꿈치 비중 측정
  } else {
    caliSumAcc += (analogRead(S_FL) + analogRead(S_FR)); // 푸쉬업은 손 하중 합 측정
  }
  sampleCount++;

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0,0);
    lcd.print("Base (3s)       ");
    lcd.setCursor(0,1);
    lcd.print("Stay Still.. "); 
    if(remain < 0) remain = 0;
    lcd.print(remain); lcd.print("s  ");
  }

  if (elapsed >= 3000) {
    if (currentExercise == SQUAT) baseRatio = caliRatioAcc / (float)sampleCount;
    else baseVal = caliSumAcc / (float)sampleCount;
    
    systemState = CALIBRATION;
    caliStep = 0; motionState = 0;
    lcd.clear();
  }
}

void handleCalibration() {
  float current = (currentExercise == SQUAT) ? getSquatRatio() : (analogRead(S_FL) + analogRead(S_FR));
  float reference = (currentExercise == SQUAT) ? baseRatio : baseVal;
  
  float diff = current - reference;
  float trigger = (currentExercise == SQUAT) ? 0.12 : 50.0; // 스쿼트는 비중 12% 차이 기준

  if (motionState == 0 && diff > trigger) {
    motionState = 1;
    caliPeaks[caliStep] = current; 
  } 
  else if (motionState == 1 && diff < trigger * 0.5) {
    caliStep++;
    motionState = 0; 
    delay(500);
  }

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0,0);
    lcd.print("Cali Step: "); lcd.print(caliStep + 1);
    lcd.print("/3  ");
    lcd.setCursor(0,1);
    lcd.print("Do 1 Rep!       ");
  }

  if (caliStep >= 3) {
    activeTh = (caliPeaks[0] + caliPeaks[1] + caliPeaks[2]) / 3.0;
    systemState = NORMAL;
    exerciseCount = 0;
    display.showNumberDec(0);
    lcd.clear();
  }
}

void handleNormal() {
  float current = (currentExercise == SQUAT) ? getSquatRatio() : (analogRead(S_FL) + analogRead(S_FR));
  float reference = (currentExercise == SQUAT) ? baseRatio : baseVal;
  
  float range = activeTh - reference;
  if (currentExercise == SQUAT && range < 0.05) range = 0.05;
  if (currentExercise == PUSHUP && range < 40) range = 40;

  float downTh = reference + range * 0.75;
  float upTh   = reference + range * 0.35; 

  if (motionState == 0 && current >= downTh) {
    motionState = 1; 
  } 
  else if (motionState == 1 && current <= upTh) {
    motionState = 0; 
    exerciseCount++;
    display.showNumberDec(exerciseCount);
    delay(500); 
  }

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    lcd.setCursor(0,0);
    lcd.print(modeNames[currentExercise-1]);
    lcd.print(" CNT: "); lcd.print(exerciseCount);
    lcd.print("    "); 
    lcd.setCursor(0,1);
    if (currentExercise == SQUAT) {
      lcd.print("R:"); lcd.print(current, 2);
      lcd.print(" B:"); lcd.print(baseRatio, 2);
    } else {
      lcd.print("V:"); lcd.print((int)current);
      lcd.print(" B:"); lcd.print((int)baseVal);
    }
    lcd.print("    "); 
  }
}

void checkResetLogic() {
  if (digitalRead(RESET_BTN) == LOW && digitalRead(MODE_BTN) == LOW) {
    delay(500);
    resetAll();
  }
  else if (digitalRead(RESET_BTN) == LOW) {
    delay(300);
    exerciseCount = 0;
    display.showNumberDec(0);
  }
}

void resetAll() {
  currentExercise = MODE_SELECT;
  systemState = PREPARE;
  exerciseCount = 0;
  display.showNumberDec(0);
  lcd.clear();
}