#include <LiquidCrystal.h>
#include <IRremote.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites.h"
#include "happysad.h"
#include <EEPROM.h>

// Persistent Values
const int EEPROM_ADDR_LEVEL = 0;
const int EEPROM_ADDR_LEAVES = 1;
const int EEPROM_ADDR_PLANTTYPE = 2;
const int EEPROM_ADDR_NAME = 3;

/* Various Global Variables */
unsigned long sadStartTime = 0;  // when the plant first became sad
const unsigned long DAY_MS = 24UL * 60UL * 60UL * 1000UL; // 24 hours

bool showResetBlink = false;
unsigned long blinkTimer = 0;
bool showResetMessage = true;

bool sadBeepPlayed = false;

int plantType;

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

int pumpPin = 10;
int buzzerPin = 8;

int arrowTones[4] = {800, 1200, 600, 1000}; // UP, RIGHT, DOWN, LEFT

String plantNames[] = {"Sprout", "Pickle", "Ivy", "Lily", "Flora", "Fiddle"};
const int numNames = sizeof(plantNames) / sizeof(plantNames[0]);
int nameSelection = 0;

int moistureSensorPin = A0;
int moistureSensorValue = 0;
int threshold;

String plantName;

//dry=540, moist=490, wet=375
int currentLeafCount = 1; // start with 1 leaf

int joyX = A1;
int joyY = A2;
int joySW = 7;

int level = 1;
const int maxSequence = 15;
int sequence[maxSequence];
int inputIndex = 0;

unsigned long lastScroll = 0;
int scrollPos = 0;
const int scrollInterval = 600;

/*Functions*/
// Determines moisture threshold based on chosed plant type
int getThresholdForType(int type) {
    switch (type) {
        case 4: return 400; // Bog/Aquatic
        case 3: return 475; // Houseplant/Fern
        case 2: return 500; // Herb
        case 1: return 540; // Succulent
        default: return 490; // fallback
    }
}

// Displays list of names for user to choose from
// Sets selected name to persistent variable
void selectPlantName() {
    bool chosen = false;

    while (!chosen) {
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_6x12_tf);
            u8g2.drawStr(0, 12, "Name your plant:");
            u8g2.drawStr(0, 32, plantNames[nameSelection].c_str());
        } while (u8g2.nextPage());

        int x = analogRead(joyX);
        int y = analogRead(joyY);
        bool btnPressed = digitalRead(joySW) == LOW;

        // Scroll selection
        if (x < 300) {
            nameSelection--;
            if (nameSelection < 0) nameSelection = numNames - 1;
            delay(200);
        } 
        else if (x > 700) {
            nameSelection++;
            if (nameSelection >= numNames) nameSelection = 0;
            delay(200);
        }

        // Select option
        if (btnPressed) {
            plantName = plantNames[nameSelection];
            chosen = true;

            u8g2.firstPage();
            do {
                u8g2.setFont(u8g2_font_6x12_tf);
                u8g2.drawStr(0, 24, "Selected:");
                u8g2.drawStr(0, 36, plantName.c_str());
            } while (u8g2.nextPage());
            delay(1500);
            EEPROM.update(EEPROM_ADDR_NAME, nameSelection);
        }
    }
}

// Displays four plant types for user to choose from
void selectPlantType() {
    String options[4] = {"Succulent", "Herb", "Houseplant/Fern", "Bog/Aquatic"};
    int selection = 0;
    bool chosen = false;

    while (!chosen) {
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_6x12_tf);
            u8g2.drawStr(0, 12, "Select your plant:");
            u8g2.drawStr(0, 32, options[selection].c_str());
        } while (u8g2.nextPage());

        int x = analogRead(joyX);
        int y = analogRead(joyY);
        bool btnPressed = digitalRead(joySW) == LOW;

        // Scroll selection
        if (x < 300) {
            selection--;
            if (selection < 0) selection = 3;
            delay(200);
        } 
        else if (x > 700) {
            selection++;
            if (selection > 3) selection = 0;
            delay(200);
        }

        // Select option
        if (btnPressed) {
            plantType = selection + 1; // 1-4
            EEPROM.update(EEPROM_ADDR_PLANTTYPE, plantType);
            threshold = getThresholdForType(plantType);
            chosen = true;

            u8g2.firstPage();
            do {
                u8g2.setFont(u8g2_font_6x12_tf);
                u8g2.drawStr(0, 24, "Selected:");
                u8g2.drawStr(0, 36, options[selection].c_str());
            } while (u8g2.nextPage());
            delay(1500);
            selectPlantName();
        }
    }
}

// Sounds tone
void beep(int frequency, int duration) {
  tone(buzzerPin, frequency, duration);
  delay(duration);
  noTone(buzzerPin);
}

// Collects proper frames from sprites.h
int getFrameForLeaf(int leafNum) {
    switch (leafNum) {
        case 1: return 0;  // frames 1-3
        case 2: return 5;  // frames 4-9
        case 3: return 9; // frames 10-12
        case 4: return 12; // frames 13-16
    }

    int extraLeaf = leafNum - 4;

    if (extraLeaf % 2 == 1) {
      return 9;
    } else {
      return 12;
    }
}

// Displays the collected frame
void drawPlantFrame(int frameIndex) {
    u8g2.firstPage();
    do {
        u8g2.drawXBMP(32, 16, 64, 32, plantFrames[frameIndex]);
    } while (u8g2.nextPage());
}

// Puts together animation
void drawPlant(int leafCount) {
    int startFrame, endFrame;

    if (leafCount == 1) {
        startFrame = 0; endFrame = 2;   // frames 1-3
    } 
    else if (leafCount == 2) {
        startFrame = 3; endFrame = 8;   // frames 4-9
    } 
    else {
        // For leafCount > 2, use odd/even rule
        if (leafCount % 2 == 1) {
            startFrame = 9; endFrame = 11;   // frames 10-12
        } else {
            startFrame = 12; endFrame = 15;  // frames 13-16
        }
    }

    for (int f = startFrame; f <= endFrame; f++) {
        drawPlantFrame(f);
        delay(50); // adjust speed of animation
    }
}

// Displays happy/sad plant from happysad.h
void drawFace(bool happy) {
  u8g2.firstPage();
  do {
    if (happy) {
      u8g2.drawXBMP(32, 16, 64, 32, happyFace);
    } else {
      u8g2.drawXBMP(32, 16, 64, 32, sadFace);
    }

  } while (u8g2.nextPage());
}

// Displays arrows for game sequence
void drawArrow(int dir) {
  u8g2.firstPage();
  do {
    switch (dir) {
      case 0: // UP
        u8g2.drawTriangle(64, 10, 54, 30, 74, 30);
        beep(arrowTones[0], 50);
        break;
      case 1: // RIGHT
        u8g2.drawTriangle(98, 32, 78, 22, 78, 42);
        beep(arrowTones[1], 50);
        break;
      case 2: // DOWN
        u8g2.drawTriangle(64, 54, 54, 34, 74, 34);
        beep(arrowTones[2], 50);
        break;
      case 3: // LEFT
        u8g2.drawTriangle(30, 32, 50, 22, 50, 42);
        beep(arrowTones[3], 50);
        break;
    }
  } while (u8g2.nextPage());
}

// Reads joystick input
int readJoystick() {
  int x = analogRead(joyX);
  int y = analogRead(joyY);

  if (digitalRead(joySW) == LOW) return 99; // future use

  if (y < 300) return 1;  // was UP -> now RIGHT
  if (x > 700) return 2;  // was RIGHT -> now DOWN
  if (y > 700) return 3;  // was DOWN -> now LEFT
  if (x < 300) return 0;  // was LEFT -> now UP

  return -1; // no move
}

// Game Play
void playGame() {
  if (level == 1) {
    u8g2.firstPage();
      do {
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(0, 12, "Repeat the following");
        u8g2.drawStr(0, 24, "arrow pattern!");
      } while (u8g2.nextPage());
      delay(3000);
    }
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

  // Generate random sequence
  for (int i = 0; i < seqLen; i++) {
    sequence[i] = random(0, 4);
  }

  // Display Sequence
  for (int i = 0; i < seqLen; i++) {
    drawArrow(sequence[i]);
    delay(600);
    u8g2.firstPage();
    do {} while(u8g2.nextPage());
    delay(250);
  }

    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB14_tr);
      u8g2.drawStr(0, 32, "Your turn!");
  } while (u8g2.nextPage());
    delay(500);

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
        // Correct direction?
        drawArrow(move);
        delay(300);
        inputIndex++;
      } else if (move >= 0 && move != sequence[inputIndex]) {
        // WRONG — level down and reset game
        beep(300, 500);

        u8g2.firstPage();
        do {
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(30, 32, "Wrong!");
        } while (u8g2.nextPage());
        delay(800);

      // Animate the leaf for the lost level
      int startFrame = getFrameForLeaf(currentLeafCount + 1);
      int endFrame = getFrameForLeaf(currentLeafCount);

      if (startFrame < endFrame) {
          for (int f = startFrame; f <= endFrame; f++) {
              drawPlantFrame(f);
              delay(100);
          }
      } else {
          for (int f = startFrame; f >= endFrame; f--) {
              drawPlantFrame(f);
              delay(100);
          }
      }

      currentLeafCount--;


        level = level-1;
        EEPROM.update(EEPROM_ADDR_LEVEL, level);
        EEPROM.update(EEPROM_ADDR_LEAVES, currentLeafCount);

          u8g2.firstPage();
          do {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawStr(30, 32, "Level: ");
            u8g2.setCursor(95, 32);
            u8g2.print(level); // level demoted after failure
          } while (u8g2.nextPage());
          delay(1000);
        return;
      }
    }
  }

  // Successfully played the game
  level++;
  currentLeafCount = level;

  EEPROM.update(EEPROM_ADDR_LEVEL, level);
  EEPROM.update(EEPROM_ADDR_LEAVES, currentLeafCount);

  u8g2.firstPage();
  do {
    beep(1500, 300);
    beep(1800, 300);
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(12, 32, "You win! Level up!");
    drawPlant(currentLeafCount);
  } while (u8g2.nextPage());

  delay(800);


  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(30, 32, "Level: ");
    u8g2.setCursor(95, 32);
    u8g2.print(level); // level increments after success
  } while (u8g2.nextPage());
  delay(1000);

  digitalWrite(pumpPin, HIGH);
  delay(5000);
  digitalWrite(pumpPin, LOW);
  lcd.clear();
}

/* Setup */
void setup() {
  u8g2.begin();
  lcd.begin(16, 2);
  Serial.begin(9600);

  level = EEPROM.read(EEPROM_ADDR_LEVEL);
  currentLeafCount = EEPROM.read(EEPROM_ADDR_LEAVES);
  plantType = EEPROM.read(EEPROM_ADDR_PLANTTYPE);
  nameSelection = EEPROM.read(EEPROM_ADDR_NAME);

  threshold = getThresholdForType(plantType);

  if (level < 1 || level > 30) level = 1;  
  if (currentLeafCount < 1 || currentLeafCount > 30) currentLeafCount = 1;
  if (plantType < 1 || plantType > 4) selectPlantType();
  if (nameSelection < 0 || nameSelection >= numNames) nameSelection = 0;
  plantName = plantNames[nameSelection];

  pinMode(joySW, INPUT_PULLUP);
  pinMode(pumpPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(pumpPin, LOW);

  randomSeed(analogRead(0));
}
/* Loop */
void loop() {
  // Factory Reset: Hold joystick down for 3 seconds
static unsigned long downStart = 0;

int x = analogRead(joyX);

if (x > 700) {  // joystick pulled down
    if (downStart == 0) {
        downStart = millis();   // start timing
    } 
    else if (millis() - downStart > 3000) {  // held for 3 seconds
        // Perform factory reset
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Factory Reset...");
        
        EEPROM.update(EEPROM_ADDR_LEVEL, 1);
        EEPROM.update(EEPROM_ADDR_LEAVES, 1);
        EEPROM.update(EEPROM_ADDR_PLANTTYPE, 0);
        EEPROM.update(EEPROM_ADDR_NAME, 0);

        level = 1;
        currentLeafCount = 1;
        plantType = 1;

        lcd.setCursor(0, 1);
        lcd.print("Done!");
        delay(1500);

        // Prevent retriggering
        downStart = millis() + 5000; 
        selectPlantType();
    }
} 
else {
    downStart = 0;  // reset timer when not being held down
}

  moistureSensorValue = analogRead(moistureSensorPin);

  // Blinking messages mode
if (showResetBlink) {
    if (millis() - blinkTimer > 2000) {
        showResetMessage = !showResetMessage;  // switch message every 2 sec
        blinkTimer = millis();
    }

    lcd.clear();

    if (showResetMessage) {
        lcd.setCursor(0, 0);
        lcd.print("Level reset due");
        lcd.setCursor(0, 1);
        lcd.print("to neglect!");
    } else {
        lcd.setCursor(0, 0);
        lcd.print(plantName + " is sad");
        lcd.setCursor(0, 1);
        lcd.print("Click to water!");
    }

    // Game play allowed
    if (digitalRead(joySW) == LOW) {
        showResetBlink = false;
        lcd.clear();
    }

    return;
}

  bool happy = (moistureSensorValue < threshold);
  drawFace(happy);

  if (!happy) {
    if (!sadBeepPlayed) {
      beep(300, 200);
      sadBeepPlayed = true;
    }
    if (sadStartTime == 0) {
      sadStartTime = millis();
    }
  } else {
    sadStartTime = 0;
    sadBeepPlayed = false;
  }

    // Check if 24 hours have passed without being watered
  if (sadStartTime > 0 && millis() - sadStartTime > DAY_MS) {
      level = 1;
      currentLeafCount = 1;
      EEPROM.update(EEPROM_ADDR_LEVEL, level);
      EEPROM.update(EEPROM_ADDR_LEAVES, currentLeafCount);
      sadStartTime = 0;

      showResetBlink = true;
      blinkTimer = millis();
      showResetMessage = true;
  }

  if (happy) {
    lcd.setCursor(0, 0);
    lcd.print(plantName + " is happy!");
  } else {

    lcd.setCursor(0, 0);
    lcd.print(plantName + " is sad   ");
    lcd.setCursor(0, 1);
    lcd.print("Click to water!");

    if (digitalRead(joySW) == LOW) {
      beep(1200, 100);
      playGame();
      lcd.clear();
      sadStartTime = 0;
    }
  }

  delay(100);

}
