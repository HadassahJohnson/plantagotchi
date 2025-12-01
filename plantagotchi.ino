#include <LiquidCrystal.h>
#include <IRremote.h>
#include <Arduino.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

int pumpPin = 10;
int buzzerPin = 8;

int arrowTones[4] = {800, 1200, 600, 1000}; // UP, RIGHT, DOWN, LEFT

int moistureSensorPin = A0;
int moistureSensorValue = 0;
int threshold = 490;

// I think my thresholds disappeared but I believe it was 540, 490, 375
String plantName = "Fiddle";

int joyX = A1;
int joyY = A2;
int joySW = 7;

int level = 1;
const int maxSequence = 15;
int sequence[maxSequence];
// int userInput[maxSequence];
int inputIndex = 0;

unsigned long lastScroll = 0;
int scrollPos = 0;
const int scrollInterval = 300;

void beep(int frequency, int duration) {
  tone(buzzerPin, frequency, duration);
  delay(duration);  // ensures the tone finishes
  noTone(buzzerPin);
}

void growPlant() {
  int baseX = 64;
  int baseY = 60; // bottom of the screen

  int stemHeight = 30;   // total height of stem
  int numLeaves = 6;

  for (int i = 0; i <= stemHeight; i++) {
    u8g2.firstPage();
    do {
      // Draw stem
      u8g2.drawLine(baseX, baseY, baseX, baseY - i);

      // Draw leaves at intervals
      for (int j = 1; j <= numLeaves; j++) {
        int leafPos = j * stemHeight / (numLeaves + 1);
        if (i >= leafPos) {
          int leafSize = 5; // length of leaf
          // Left leaf
          u8g2.drawLine(baseX, baseY - leafPos, baseX - leafSize, baseY - leafPos - leafSize);
          // Right leaf
          u8g2.drawLine(baseX, baseY - leafPos, baseX + leafSize, baseY - leafPos - leafSize);
        }
      }
    } while (u8g2.nextPage());

    delay(150); // control growth speed
  }
}

void drawFrame(int x, int y, int stage) {
  u8g2.firstPage();
  do {
    // Draw stem
    if (stage >= 1) {
      int stemHeight = min(stage, 20);
      u8g2.drawLine(x, y, x, y - stemHeight);
    }

    // Draw leaves
    if (stage > 20 && stage <= 40) {
      int leafStage = stage - 20;
      u8g2.drawLine(x, y - 10, x - leafStage, y - 15); // left leaf
      u8g2.drawLine(x, y - 15, x + leafStage, y - 20); // right leaf
    }

    // Draw flower
    if (stage > 40) {
      u8g2.drawCircle(x, y - 20, 5, U8G2_DRAW_ALL); // simple flower
    }

  } while (u8g2.nextPage());
}

void drawFace(bool happy) {
  u8g2.firstPage();
  do {
    // Face outline
    u8g2.drawCircle(64, 32, 20);

    // Eyes
    u8g2.drawDisc(56, 28, 2);
    u8g2.drawDisc(72, 28, 2);

    if (happy) {
      u8g2.drawArc(64, 36, 10, 130, 250);
    } else {
      u8g2.drawArc(64, 44, 10, 250, 130);
    }

  } while (u8g2.nextPage());
}

void drawArrow(int dir) {
  u8g2.firstPage();
  do {
    switch (dir) {
      case 0: // UP
        u8g2.drawTriangle(64, 10, 54, 30, 74, 30);
        beep(arrowTones[0], 200);
        break;
      case 1: // RIGHT
        u8g2.drawTriangle(98, 32, 78, 22, 78, 42);
        beep(arrowTones[1], 200);
        break;
      case 2: // DOWN
        u8g2.drawTriangle(64, 54, 54, 34, 74, 34);
        beep(arrowTones[2], 200);
        break;
      case 3: // LEFT
        u8g2.drawTriangle(30, 32, 50, 22, 50, 42);
        beep(arrowTones[3], 200);
        break;
    }
  } while (u8g2.nextPage());
}

int readJoystick() {
  int x = analogRead(joyX);
  int y = analogRead(joyY);

  if (digitalRead(joySW) == LOW) return 99; // future use

  if (y < 300) return 0;  // UP
  if (x > 700) return 1;  // RIGHT
  if (y > 700) return 2;  // DOWN
  if (x < 300) return 3;  // LEFT

  return -1; // no move
}

void playGame() {
  int seqLen = level + 2;
  if (seqLen > maxSequence) seqLen = maxSequence;

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(30, 31, "Ready...");
  } while (u8g2.nextPage());
  delay(1000);

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(50, 32, "GO!");
 } while (u8g2.nextPage());
  delay(500);

  // Generate sequence
  for (int i = 0; i < seqLen; i++) {
    sequence[i] = random(0, 4);
  }

  // SHOW SEQUENCE
  for (int i = 0; i < seqLen; i++) {
    drawArrow(sequence[i]);
    delay(600);
    u8g2.firstPage();
    do {} while(u8g2.nextPage());
    delay(250);
  }

  // USER INPUT
  inputIndex = 0;

  while (inputIndex < seqLen) {
    int move = readJoystick();

    //up = 0, right = 1, down = 2, left = 3
    if (move >= 0) {
      Serial.print(" Joystick: ");
      Serial.print(move);
      Serial.print("| Expected: ");
      Serial.print(sequence[inputIndex]);

      if (move == sequence[inputIndex]) {
        // Correct direction
        drawArrow(move);
        delay(300);
        inputIndex++;
      } else if (move >= 0 && move != sequence[inputIndex]) {
        // WRONG — reset game
        beep(300, 500);
        u8g2.firstPage();
        do {
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(30, 32, "Wrong!");
        } while (u8g2.nextPage());
        delay(800);
        level = 1;
        return;
      }
    }
  }

  // If we reach here — SUCCESS
  u8g2.firstPage();
  do {
    beep(1500, 300);
    beep(1800, 300);
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(12, 32, "You win! Level up!");
    growPlant();
  } while (u8g2.nextPage());

  delay(800);
  level++;

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(30, 32, "Level: ");
    u8g2.setCursor(95, 32);
    u8g2.print(level + 1); // level increments after success
  } while (u8g2.nextPage());
  delay(1000);

  digitalWrite(pumpPin, HIGH);
  delay(5000);
  digitalWrite(pumpPin, LOW);
}

void setup() {
  u8g2.begin();
  lcd.begin(16, 2);
  Serial.begin(9600);

  pinMode(joySW, INPUT_PULLUP);
  pinMode(pumpPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(pumpPin, LOW);

  randomSeed(analogRead(0));
}

void loop() {
  moistureSensorValue = analogRead(moistureSensorPin);

  lcd.setCursor(0, 0);
  lcd.print("Moisture: ");
  lcd.print(moistureSensorValue);
  lcd.print("    ");

  bool happy = (moistureSensorValue < threshold);
  drawFace(happy);

  if (happy) {
    lcd.setCursor(0, 1);
    lcd.print("Plant is happy!");
    delay(500);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Plant is dry :(  Press the joystick to play the game!");
    if (millis() - lastScroll >= scrollInterval) {
      lcd.scrollDisplayLeft();
      lastScroll = millis();
    }
    delay(50);
    if (digitalRead(joySW) == LOW) {
      beep(1200, 100);
      playGame();
    }
  }

  delay(100);

}