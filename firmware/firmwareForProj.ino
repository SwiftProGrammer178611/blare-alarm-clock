#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>

//initializaitons
#define DHT_PIN       0     
#define piezoPin    8     
#define DHT_TYPE      DHT11

// ST7789 TFT Pins 
#define TFT_CS        20
#define TFT_DC        10
#define TFT_RST       8 // Shared with buzzer, might click a bit on boot lol

// buttons directly to gnd
const int BTN_SET_PAGE   = 2;
const int BTN_THEME      = 3;
const int BTN_MODE       = 4;
const int BTN_PADDLE_UP  = 5;
const int BTN_PADDLE_DN  = 20;
const int BTN_COMPANION  = 21;

// oled addresses
#define OLED_ADDR_1 0x3C  // settings
#define OLED_ADDR_2 0x3D  // weather
#define OLED_ADDR_3 0x3C  // companion face

Adafruit_SSD1306 oled1(128, 64, &Wire, -1);
Adafruit_SSD1306 oled2(128, 64, &Wire, -1);
Adafruit_SSD1306 oled3(128, 64, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// gb vars
enum SystemMode { MODE_CLOCK, MODE_GAME };
SystemMode currentMode = MODE_CLOCK;

// time tracker (change this to wifi ntp later!!)
int hours = 12;
int minutes = 0;
int seconds = 0;
unsigned long lastTimeTick = 0;

int activeTheme = 0; // 0=cyberpunk, 1=retro green
int brightnessSetting = 100;

float currentTemp = 0.0;
float currentHumidity = 0.0;
unsigned long lastSensorCheck = 0;

int cpExpression = 0; // 0=happy, 1=blink, 2=slant
unsigned long lastExpressionChange = 0;

// game vars
int playerY = 40;
int ballX = 80;
int ballY = 60;
int ballDX = 3;
int ballDY = 2;
const int paddleH = 25;
const int paddleW = 4;

void keepInternalTime() {
  if (millis() - lastTimeTick >= 1000) {
    seconds++;
    if (seconds >= 60) { 
      seconds = 0; 
      minutes++; 
    }
    if (minutes >= 60) { 
      minutes = 0; 
      hours++; 
    }
    if (hours >= 24) { 
      hours = 0; 
    }
    lastTimeTick = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(0, 1); // hardware pins 0 and 1
  
  pinMode(BTN_SET_PAGE, INPUT_PULLUP);
  pinMode(BTN_THEME, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_PADDLE_UP, INPUT_PULLUP);
  pinMode(BTN_PADDLE_DN, INPUT_PULLUP);
  pinMode(piezoPin, OUTPUT);

  // init
  tft.init(135, 240); 
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  
  oled1.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_1);
  oled2.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_2);
  oled3.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_3);
  
  dht.begin();
}

void drawClockFace() {
  tft.setTextWrap(false);
  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.setTextColor(activeTheme == 0 ? ST77XX_CYAN : ST77XX_GREEN, ST77XX_BLACK);
  tft.print("DESKTOP DASHBOARDOS");
  
  String ckTime = "";
  if(hours < 10) ckTime += "0";
  ckTime += String(hours) + ":";
  if(minutes < 10) ckTime += "0";
  ckTime += String(minutes) + ":";
  if(seconds < 10) ckTime += "0";
  ckTime += String(seconds);
  
  tft.setCursor(40, 50);
  tft.setTextSize(4);
  tft.print(ckTime);
}

void runPingPongLogic() {
  
  tft.fillCircle(ballX, ballY, 3, ST77XX_BLACK);
  tft.fillRect(5, 0, paddleW, 135, ST77XX_BLACK); // hardcoded height here fix later

  ballX += ballDX; 
  ballY += ballDY;
  
  if (ballY <= 0 || ballY >= 135) { 
    ballDY = -ballDY;
  }
  if (ballX >= 240) { 
    ballDX = -ballDX;
  }

  // collision check
  if (ballX <= (5 + paddleW) && ballY >= playerY && ballY <= (playerY + paddleH)) {
    ballDX = -ballDX;
    tone(piezoPin, 1500, 20); 
  }

  if (ballX < 0) { 
    ballX = 120;
    ballY = 67;
    ballDX = -ballDX;
    tone(piezoPin, 300, 150); 
  }

  uint16_t paddleColor = activeTheme == 0 ? ST77XX_MAGENTA : ST77XX_GREEN;
  tft.fillRect(5, playerY, paddleW, paddleH, paddleColor);
  tft.fillCircle(ballX, ballY, 3, ST77XX_WHITE);
}

void renderOLEDs() {
  // --- OLED 1 (SETTINGS) ---
  oled1.clearDisplay();
  oled1.setTextSize(1);
  oled1.setTextColor(SSD1306_WHITE);
  oled1.setCursor(0,0);
  oled1.println("SYSTEM SETTINGS:");
  oled1.print("Theme: "); oled1.println(activeTheme == 0 ? "Cyberpunk" : "Retro");
  oled1.print("Brightness: "); oled1.print(brightnessSetting); oled1.println("%");
  oled1.display();

  // --- OLED 2 (WEATHER) ---
  oled2.clearDisplay();
  oled2.setTextSize(1);
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setCursor(0,0);
  oled2.println("DHT11 ENV READINGS");
  oled2.print("Temp: "); oled2.print(currentTemp); oled2.println(" C");
  oled2.print("Humid: "); oled2.print(currentHumidity); oled2.println(" %");
  oled2.display();

  // --- OLED 3 (COZMO COMPANION FACE) ---
  oled3.clearDisplay();
  if (cpExpression == 0) { // happy
    oled3.fillRect(30, 20, 20, 30, SSD1306_WHITE);
    oled3.fillRect(78, 20, 20, 30, SSD1306_WHITE);
  } else if (cpExpression == 1) { // blink
    oled3.fillRect(30, 32, 20, 6, SSD1306_WHITE);
    oled3.fillRect(78, 32, 20, 6, SSD1306_WHITE);
  } else { // slanted eyes
    oled3.fillTriangle(30,20, 50,35, 30,50, SSD1306_WHITE);
    oled3.fillTriangle(98,20, 78,35, 98,50, SSD1306_WHITE);
  }
  oled3.display();
}

void readInputs() {
  if (digitalRead(BTN_MODE) == LOW) {
    tone(piezoPin, 800, 40);
    if(currentMode == MODE_CLOCK) {
      currentMode = MODE_GAME;
    } else {
      currentMode = MODE_CLOCK;
    }
    tft.fillScreen(ST77XX_BLACK);
    delay(200); 
  }

  if (digitalRead(BTN_THEME) == LOW) {
    tone(piezoPin, 1200, 30);
    activeTheme = !activeTheme;
    delay(200);
  }

  if (currentMode == MODE_GAME) {
    if (digitalRead(BTN_PADDLE_UP) == LOW && playerY > 0) {
      playerY -= 4;
    }
    if (digitalRead(BTN_PADDLE_DN) == LOW && playerY < (135 - paddleH)) {
      playerY += 4;
    }
  }
}

void loop() {
  readInputs();
  keepInternalTime();

  if (millis() - lastSensorCheck > 3000) {
    currentTemp = dht.readTemperature();
    currentHumidity = dht.readHumidity();
    lastSensorCheck = millis();
  }

  if (millis() - lastExpressionChange > 4000) {
    cpExpression = random(0, 3);
    lastExpressionChange = millis();
  }

  renderOLEDs();
  
  if (currentMode == MODE_CLOCK) {
    drawClockFace();
  } else if (currentMode == MODE_GAME) {
    runPingPongLogic();
  }
}
