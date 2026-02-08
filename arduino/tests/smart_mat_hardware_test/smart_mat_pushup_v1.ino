#include <LiquidCrystal.h>
#include <TM1637Display.h>

/* =========================
   디스플레이 설정
========================= */
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

/* =========================
   센서 설정
========================= */
const int fsrPins[4] = {A0, A1, A2, A3};
int fsrValues[4];
int sumPressure = 0;

/* =========================
   푸쉬업 상태 정의
========================= */
enum PushUpState {
  READY,
  DOWN
};

enum SystemMode {
  CALIBRATION,
  NORMAL
};

PushUpState pushState = READY;
SystemMode mode = CALIBRATION;

/* =========================
   캘리브레이션 변수
========================= */
const int CALI_TARGET = 3;
int caliCount = 0;
int downPeaks[3];
int currentDownPeak = 0;
float downAvg = 0;

/* =========================
   실전 변수
========================= */
int pushUpCount = 0;
bool isActive = false;

/* 비율 기준 */
const float DOWN_RATIO = 0.8;
const float UP_RATIO   = 0.4;

void setup() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Smart Mat");

  display.setBrightness(0x0f);
  display.showNumberDec(0);

  delay(1000);
  lcd.clear();
}

void loop() {
  /* 1️⃣ 센서 읽기 */
  sumPressure = 0;
  for (int i = 0; i < 4; i++) {
    fsrValues[i] = analogRead(fsrPins[i]);
    sumPressure += fsrValues[i];
  }

  /* =========================
     CALIBRATION MODE
  ========================= */
  if (mode == CALIBRATION) {

    lcd.setCursor(0, 0);
    lcd.print("Calibrating ");
    lcd.print(caliCount + 1);
    lcd.print("/3   ");

    lcd.setCursor(0, 1);
    lcd.print("SUM:");
    lcd.print(sumPressure);
    lcd.print("   ");

    /* READY → DOWN */
    if (pushState == READY && sumPressure > 300) {
      pushState = DOWN;
      currentDownPeak = sumPressure;
    }

    /* DOWN 상태에서 최대값 추적 */
    if (pushState == DOWN) {
      if (sumPressure > currentDownPeak) {
        currentDownPeak = sumPressure;
      }

      /* DOWN → READY (올라옴) */
      if (sumPressure < 200) {
        downPeaks[caliCount] = currentDownPeak;
        caliCount++;

        pushState = READY;
        currentDownPeak = 0;

        delay(300); // 디바운스
      }
    }

    /* 캘리브레이션 완료 */
    if (caliCount >= CALI_TARGET) {
      downAvg = (downPeaks[0] + downPeaks[1] + downPeaks[2]) / 3.0;
      mode = NORMAL;

      lcd.clear();
      lcd.print("Calibration OK");
      delay(1000);
      lcd.clear();
    }

    display.showNumberDec(0);
    delay(50);
    return;
  }

  /* =========================
     NORMAL MODE
  ========================= */
  float downThreshold = downAvg * DOWN_RATIO;
  float upThreshold   = downAvg * UP_RATIO;

  lcd.setCursor(0, 0);
  lcd.print("SUM:");
  lcd.print(sumPressure);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  lcd.print("CNT:");
  lcd.print(pushUpCount);
  lcd.print("   ");

  /* READY → DOWN */
  if (pushState == READY && sumPressure > downThreshold) {
    pushState = DOWN;
  }

  /* DOWN → READY = 1회 성공 */
  if (pushState == DOWN && sumPressure < upThreshold) {
    pushState = READY;
    pushUpCount++;
  }

  display.showNumberDec(pushUpCount);
  delay(80);
}
