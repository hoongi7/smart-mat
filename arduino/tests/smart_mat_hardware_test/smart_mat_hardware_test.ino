#include <LiquidCrystal.h>
#include <TM1637Display.h>

// 배선한 핀 번호 그대로 설정
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

const int modeBtn = 2;
const int resetBtn = 3;
const int greenLED = 6;
const int redLED = 13;

int testCounter = 0;

void setup() {
  // LCD 초기화
  lcd.begin(16, 2);
  lcd.print("Hardware Check");
  
  // 7세그먼트 초기화
  display.setBrightness(0x0f);
  
  // 버튼 및 LED 핀 설정
  pinMode(modeBtn, INPUT_PULLUP);
  pinMode(resetBtn, INPUT_PULLUP);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  delay(1000);
  lcd.clear();
}

void loop() {
  // 1. LCD 및 7세그먼트 테스트 (숫자 증가)
  testCounter++;
  lcd.setCursor(0, 0);
  lcd.print("LCD Test: ");
  lcd.print(testCounter);
  
  display.showNumberDec(testCounter % 10000);

  // 2. LED 교차 점멸 테스트
  if (testCounter % 2 == 0) {
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
  } else {
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);
  }

  // 3. 스위치(버튼) 입력 테스트
  lcd.setCursor(0, 1);
  if (digitalRead(modeBtn) == LOW) {
    lcd.print("BTN 1: OK!      ");
  } else if (digitalRead(resetBtn) == LOW) {
    lcd.print("BTN 2: OK!      ");
  } else {
    lcd.print("Wait for Button ");
  }

  delay(200); // 눈으로 확인할 수 있는 속도로 갱신
}