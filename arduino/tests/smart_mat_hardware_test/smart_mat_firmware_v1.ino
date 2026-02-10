#include <LiquidCrystal.h>
#include <TM1637Display.h>

/* =========================
   디스플레이
========================= */
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
TM1637Display display(4, 5);

/* =========================
   버튼
========================= */
const int RESET_BTN = 2;   // 확정 / 리셋
const int MODE_BTN  = 3;   // 모드 이동

/* =========================
   압력 센서
========================= */
const int fsrPins[4] = {A0, A1, A2, A3};
int fsrValues[4];
int sumPressure = 0;

/* =========================
   상태 정의
========================= */
enum ExerciseMode {
  MODE_SELECT,
  PUSHUP,
  SQUAT,
  PLANK
};

enum SystemState {
  CALIBRATION,
  NORMAL
};

// 현재 상태 변수
ExerciseMode currentExercise = MODE_SELECT; 
SystemState systemState = CALIBRATION;

/* =========================
   모드 선택
========================= */
int selectedIndex = 0;
const char* modeNames[] = {"PUSH-UP", "SQUAT", "PLANK"};

/* =========================
   PUSH-UP 변수
========================= */
enum PushState { READY, DOWN };
PushState pushState = READY;

const int CALI_TARGET = 3;    // 학습단계에서 3회 측정인거
int caliCount = 0;    // 학습단계에서 지금 몇회째 인지
int downPeaks[3];   // 가장 아래로내려갔을때 합 배열
int currentPeak = 0;    // 푸쉬업은 내려갈수록 압력 쎄짐. 그래서 계속 측정하다가 제일 압력쏄때가 가장 낮은 지점으로 판단
float downAvg = 0;    // 완전히 내려갔을때 기준값

int pushUpCount = 0;

/* 기준 비율 */
const float DOWN_RATIO = 0.8;   // 80퍼만 내려가도 내려간걸로 ㅇㅈ
const float UP_RATIO   = 0.4;   // 대충 40퍼만 올라와도 올라온걸로 ㅇㅈ

/* =========================
   SETUP
========================= */
void setup() {
  lcd.begin(16, 2);
  display.setBrightness(0x0f);
  display.showNumberDec(0);

  pinMode(RESET_BTN, INPUT_PULLUP);   // 버튼이 눌리지 않았을 때 입력이 흔들리지 않도록 아두이노가 내부 저항으로 핀을 안정시키는 설정
  pinMode(MODE_BTN, INPUT_PULLUP);  // LOW는 눌렸을때, HIGH는 안눌렸을때 라고 이해

  lcd.print("Smart Mat");
  delay(1000);
  lcd.clear();
}

/* =========================
   LOOP
========================= */
void loop() {
  readSensors();  // 항상 최신정보 확보

  switch (currentExercise) {
    case MODE_SELECT:
      handleModeSelect();
      break;

    case PUSHUP:
      handlePushUp();
      break;

    case SQUAT:
      lcd.setCursor(0, 0);
      lcd.print("SQUAT MODE    ");    // 이거마저짜야한다잉
      break;

    case PLANK:
      lcd.setCursor(0, 0);
      lcd.print("PLANK MODE    ");    // 이것도요
      break;
  }
}

/* =========================
   센서 읽기
========================= */
void readSensors() {
  sumPressure = 0;
  for (int i = 0; i < 4; i++) {
    fsrValues[i] = analogRead(fsrPins[i]);    // 지금 이 순간 매트 위에 걸린 압력을 숫자로 스냅샷 찍는 함수
    sumPressure += fsrValues[i];
  }
}

/* =========================
   MODE 선택
========================= */
void handleModeSelect() {
  lcd.setCursor(0, 0);
  lcd.print("SELECT MODE   ");  // 첫째줄에 이거 출력

  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(modeNames[selectedIndex]);
  lcd.print("   ");

  // MODE 버튼 → 선택지 이동
  if (digitalRead(MODE_BTN) == LOW) {
    delay(200); // 오류 방지
    selectedIndex = (selectedIndex + 1) % 3;  // %쓴거면 느낌오지?
  }

  // RESET 버튼 → 선택 확정
  if (digitalRead(RESET_BTN) == LOW) {  
    delay(200);

    resetPushUp();           // 이전운동상태리셋        (추후 운동별 reset 함수로 분기)
    systemState = CALIBRATION;

    if (selectedIndex == 0) currentExercise = PUSHUP;
    if (selectedIndex == 1) currentExercise = SQUAT;
    if (selectedIndex == 2) currentExercise = PLANK;

    lcd.clear();
  }
}

/* =========================
   PUSH-UP 로직
========================= */
void handlePushUp() {

  bool resetPressed = (digitalRead(RESET_BTN) == LOW);
  bool modePressed  = (digitalRead(MODE_BTN)  == LOW);    // 우선 버튼상태 읽고

  /* ===== RESET + MODE : 캘리브레이션부터 다시 ===== */
  if (resetPressed && modePressed) {
    delay(300);
    resetPushUp();
    systemState = CALIBRATION;
    lcd.clear();
    return;
  }

  /* ===== RESET 단독 ===== */
  if (resetPressed) {
    delay(200);

    if (systemState == CALIBRATION) {
      // 캘리 중 → 캘리 0회부터
      resetPushUp();
      systemState = CALIBRATION;
    }
    else if (systemState == NORMAL) {
      // 실전 중 → 카운트만 리셋 (기준 유지)
      pushUpCount = 0;
      pushState = READY;
    }

    lcd.clear();
    return;
  }

  /* =========================
     CALIBRATION MODE
  ========================= */
  if (systemState == CALIBRATION) {

    lcd.setCursor(0, 0);
    lcd.print("Calibrating ");
    lcd.print(caliCount + 1);
    lcd.print("/3   ");

    lcd.setCursor(0, 1);
    lcd.print("SUM:");
    lcd.print(sumPressure);
    lcd.print("   ");

    if (pushState == READY && sumPressure > 300) {  // 레디 했고, 무게 좀 실리면
      pushState = DOWN;
      currentPeak = sumPressure;  // 초기값 설정
    }

    if (pushState == DOWN) {
      if (sumPressure > currentPeak)
        currentPeak = sumPressure;  // 가장 눌린 순간 찾기

      if (sumPressure < 200) {    
        downPeaks[caliCount] = currentPeak;   // 횟수마다 배열에 저장
        caliCount++;    // ㅇㅇ 이제 하나 더하고
        pushState = READY;    // 새로 감지해야하니깐 다시 레디
        currentPeak = 0;    // ㅇㅇ 당연히 피크값도 초기화해야함 그래야 기준이 안깨짐
        delay(300);
      }
    }

    if (caliCount >= CALI_TARGET) {   // 3번 다했으면
      downAvg = (downPeaks[0] + downPeaks[1] + downPeaks[2]) / 3.0;    // 평균계산 ㅋㅋ 
      systemState = NORMAL;   // 이제 실전모드 시작이노

      lcd.clear();
      lcd.print("Calibration OK");
      delay(1000);     // ㅇㅇ LCD에 이제 표시요
      lcd.clear();
    }

    display.showNumberDec(0);   // 캘리땐 횟수개념 없으니깐 7세그엔 안나타냄요
    return;
  }

  /* =========================
     NORMAL MODE
  ========================= */
  float downTh = downAvg * DOWN_RATIO;
  float upTh   = downAvg * UP_RATIO;

  lcd.setCursor(0, 0);
  lcd.print("SUM:");
  lcd.print(sumPressure);
  lcd.print("   ");   // 실시간 양손/양발 압력 합

  lcd.setCursor(0, 1);
  lcd.print("CNT:");
  lcd.print(pushUpCount);
  lcd.print("   ");   // 완성된 푸쉬업 수 

  if (pushState == READY && sumPressure > downTh) {    // 내려가기시작하면
    pushState = DOWN;   // 내려간상태
  }

  if (pushState == DOWN && sumPressure < upTh) {    
    pushState = READY;
    pushUpCount++;
  }

  display.showNumberDec(pushUpCount);     // 7세그 표시
}

/* =========================
   PUSH-UP 초기화
========================= */
void resetPushUp() {
  caliCount = 0;
  pushUpCount = 0;
  pushState = READY;
  currentPeak = 0;
  downAvg = 0;
}
