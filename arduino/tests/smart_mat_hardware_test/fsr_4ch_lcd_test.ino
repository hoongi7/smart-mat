#include <LiquidCrystal.h>

// LCD 핀 설정 (네가 쓰던 그대로)
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// 압력센서 핀 배열
const int fsrPins[4] = {A0, A1, A2, A3};
int fsrValues[4] = {0, 0, 0, 0};

void setup() {
  lcd.begin(16, 2);
  lcd.print("FSR Test 4ch");
}

void loop() {
  // 1. 센서 값 읽기
  for (int i = 0; i < 4; i++) {
    fsrValues[i] = analogRead(fsrPins[i]);
  }

  // 2. LCD 첫 줄 (센서 1, 2)
  lcd.setCursor(0, 0);
  lcd.print("F1:");
  lcd.print(fsrValues[0]);
  lcd.print(" F2:");
  lcd.print(fsrValues[1]);
  lcd.print("  ");   // 찌꺼기 제거

  // 3. LCD 두 번째 줄 (센서 3, 4)
  lcd.setCursor(0, 1);
  lcd.print("F3:");
  lcd.print(fsrValues[2]);
  lcd.print(" F4:");
  lcd.print(fsrValues[3]);
  lcd.print("  ");   // 찌꺼기 제거

  delay(200); // 보기 좋은 속도
}
