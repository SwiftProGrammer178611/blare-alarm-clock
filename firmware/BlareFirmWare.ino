#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ST7789.h>

#define row1 9   
#define row2 3
#define col1 4
#define col2 5
#define col3 1
#define piezoPin 2   
#define sda 6
#define scl 7
#define cs   20
#define dc   10
#define mosi 8    
#define sck  21   
#define rst  -1   
#define backlt   0  
#define settingsOledAdrs 0x3C
#define gameOledAdrs     0x3D

Adafruit_SSD1306 oledSettings(128, 64, &Wire, -1);
Adafruit_SSD1306 oledGame(128, 64, &Wire, -1);
Adafruit_ST7789 tft = Adafruit_ST7789(cs, dc, mosi, sck, rst);

const int rowPins[2] = { row1, row2 };
const int colPins[3] = { col1, col2, col3 };
bool matrixState[2][3];

// ck
int hh = 7; 
int mm = 0; 
int ss = 0;
unsigned long lasttick = 0;

// settigns
int theme = 0;
int brightPct = 100;
bool configActive = false;

// alarm
int alarmHour = 7; 
int alarmMinute = 30;
bool alarmEnabled = false;
int alarmSetState = 0;
bool isAlarmRing = false;
unsigned long lastBeep = 0;

// pingpong
int padY = 24; 
int cpuPadY = 24;
int xBall = 64; 
int yBall = 32;
int xVel = 2; 
int yVel = 1;
int scorep = 0;
int scoreC = 0;

// sleep
unsigned long lastActivity = 0;
bool sleeping = false;
const unsigned long sleepMS = 60000UL;

void scanMatrix() {
  for (int r = 0; r < 2; r++) pinMode(rowPins[r], INPUT);
  for (int r = 0; r < 2; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(10);
    for (int c = 0; c < 3; c++) matrixState[r][c] = (digitalRead(colPins[c]) == LOW);
    pinMode(rowPins[r], INPUT);
  }
}

void applyBrightness(int pct) {
  uint8_t v = map(constrain(pct, 0, 100), 0, 100, 0, 255);
  oledSettings.ssd1306_command(SSD1306_SETCONTRAST);
  oledSettings.ssd1306_command(v);
  oledGame.ssd1306_command(SSD1306_SETCONTRAST);
  oledGame.ssd1306_command(v);
  analogWrite(backlt, v); 
}

void wake() {
  if (sleeping) {
    sleeping = false;
    oledSettings.ssd1306_command(SSD1306_DISPLAYON);
    oledGame.ssd1306_command(SSD1306_DISPLAYON);
  }
  lastActivity = millis();
}

void resetBall() {
  xBall = 64;
  yBall = 32;
  xVel = -xVel;
}

void setup() {
  Wire.begin(sda, scl);

  for (int c = 0; c < 3; c++) pinMode(colPins[c], INPUT_PULLUP);
  for (int r = 0; r < 2; r++) pinMode(rowPins[r], INPUT); 
  pinMode(piezoPin, OUTPUT);
  pinMode(backlt, OUTPUT);

  tft.init(135, 240);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  oledSettings.begin(SSD1306_SWITCHCAPVCC, settingsOledAdrs);
  oledGame.begin(SSD1306_SWITCHCAPVCC, gameOledAdrs);
  applyBrightness(brightPct);

  lastActivity = millis();
}

void loop() {
  scanMatrix();
  bool cfgP    = matrixState[0][0];
  bool themeP  = matrixState[0][1];
  bool switchP = matrixState[0][2];
  bool upP     = matrixState[1][0];
  bool downP   = matrixState[1][1];
  bool wakeP   = matrixState[1][2];

  bool wasSleeping = sleeping;
  bool anyBtn = cfgP || themeP || switchP || upP || downP || wakeP;
  if (anyBtn) wake();
  if (isAlarmRing && anyBtn) isAlarmRing = false; 

  if (sleeping) return;

  if (wakeP && !wasSleeping) { 
    sleeping = true;
    tft.fillScreen(ST77XX_BLACK);
    oledSettings.ssd1306_command(SSD1306_DISPLAYOFF);
    oledGame.ssd1306_command(SSD1306_DISPLAYOFF);
    delay(200);
    return;
  }
  if (!isAlarmRing && millis() - lastActivity > sleepMS) { 
    sleeping = true;
    tft.fillScreen(ST77XX_BLACK);
    oledSettings.ssd1306_command(SSD1306_DISPLAYOFF);
    oledGame.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  if (millis() - lasttick >= 1000) {
    ss++;
    if (ss >= 60) { ss = 0; mm++; }
    if (mm >= 60) { mm = 0; hh++; }
    if (hh >= 24) hh = 0;
    lasttick = millis();
  }

  if (alarmEnabled && !isAlarmRing && hh == alarmHour && mm == alarmMinute && ss == 0) {
    isAlarmRing = true;
  }
  if (isAlarmRing && millis() - lastBeep > 400) {
    tone(piezoPin, 1000, 200);
    lastBeep = millis();
  }

  if (switchP) {
    tone(piezoPin, 800, 40);
    alarmSetState = (alarmSetState + 1) % 4;
    delay(200);
  }
  if (themeP) {
    tone(piezoPin, 1200, 30);
    theme = !theme;
    delay(200);
  }
  if (cfgP) {
    tone(piezoPin, 1000, 30);
    configActive = !configActive;
    delay(200);
  }

  if (configActive) {
    if (upP)   { brightPct = min(100, brightPct + 10); delay(150); }
    if (downP) { brightPct = max(0, brightPct - 10); delay(150); }
    applyBrightness(brightPct);
  } else if (alarmSetState == 1) {                       
    if (upP)   { alarmHour = (alarmHour + 1) % 24; delay(150); }
    if (downP) { alarmHour = (alarmHour + 23) % 24; delay(150); }
  } else if (alarmSetState == 2) {                      
    if (upP)   { alarmMinute = (alarmMinute + 1) % 60; delay(150); }
    if (downP) { alarmMinute = (alarmMinute + 59) % 60; delay(150); }
  } else if (alarmSetState == 3) {                      
    if (upP)   { alarmEnabled = true; delay(150); }
    if (downP) { alarmEnabled = false; delay(150); }
  } else if (!configActive) {                            
    if (upP && padY > 0) padY -= 3;
    if (downP && padY < 48) padY += 3;
  }
  if (!configActive && alarmSetState == 0) {
    if (cpuPadY + 8 < yBall && cpuPadY < 48) cpuPadY += 2;
    if (cpuPadY + 8 > yBall && cpuPadY > 0)  cpuPadY -= 2;

    xBall += xVel;
    yBall += yVel;
    if (yBall <= 0 || yBall >= 63) yVel = -yVel;

    if (xBall <= 5 && yBall >= padY && yBall <= padY + 16) {
      xVel = -xVel;
      tone(piezoPin, 1500, 20);
    }
    if (xBall >= 122 && yBall >= cpuPadY && yBall <= cpuPadY + 16) {
      xVel = -xVel;
      tone(piezoPin, 600, 20);
    }
    if (xBall < 0)   { scoreC++; resetBall(); }
    if (xBall > 127) { scorep++; resetBall(); }
  }

  
  oledSettings.clearDisplay();
  oledSettings.setTextSize(1);
  oledSettings.setTextColor(SSD1306_WHITE);
  oledSettings.setCursor(0, 0);
  oledSettings.println("SETTINGS");
  oledSettings.print("Theme: ");
  if (theme == 0) {
    oledSettings.println("Cyberpunk");
  } else {
    oledSettings.println("Retro");
  }
  oledSettings.print("Brightness: ");
  oledSettings.print(brightPct);
  oledSettings.println("%");
  if (configActive) oledSettings.println("[Up/Down to adjust]");
  oledSettings.display();

  
  oledGame.clearDisplay();
  oledGame.setTextSize(1);
  oledGame.setTextColor(SSD1306_WHITE);
  oledGame.setCursor(0, 0);
  oledGame.print("You "); oledGame.print(scorep);
  oledGame.print("  CPU "); oledGame.println(scoreC);
  oledGame.fillRect(2, padY, 3, 16, SSD1306_WHITE);
  oledGame.fillRect(123, cpuPadY, 3, 16, SSD1306_WHITE);
  oledGame.fillRect(xBall, yBall, 3, 3, SSD1306_WHITE);
  oledGame.display();

  
  if (configActive) {
    
  } else {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);
    if (theme == 0) {
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    }

    tft.setCursor(10, 10);
    tft.setTextSize(2);
    tft.print("ALARM CLOCK");

    String timeStr = "";
    if (hh < 10) timeStr += "0";
    timeStr += String(hh) + ":";
    if (mm < 10) timeStr += "0";
    timeStr += String(mm) + ":";
    if (ss < 10) timeStr += "0";
    timeStr += String(ss);

    tft.setCursor(40, 50);
    tft.setTextSize(4);
    tft.print(timeStr);

    tft.setTextSize(2);
    tft.setCursor(10, 100);
    if (alarmSetState == 1) {
      tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
      tft.printf("SET HOUR: %02d", alarmHour);
    } else if (alarmSetState == 2) {
      tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
      tft.printf("SET MIN: %02d", alarmMinute);
    } else if (alarmSetState == 3) {
      tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
      if (alarmEnabled) {
        tft.print("ALARM: ON");
      } else {
        tft.print("ALARM: OFF");
      };
    } else {
      if (theme == 0) {
        tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
      } else {
        tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      }
      if (alarmEnabled) {
        tft.printf("Alarm %02d:%02d ON", alarmHour, alarmMinute);
      } else {
        tft.printf("Alarm %02d:%02d OFF", alarmHour, alarmMinute);
      }
    }

    if (isAlarmRing) {
      tft.setCursor(10, 130);
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.print("*** RINGING - press any key ***");
    }
  }
}
