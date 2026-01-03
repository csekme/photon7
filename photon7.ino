#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define LDR_PIN 34
#define LED_PIN 25
#define BTN1_PIN 32
#define BTN2_PIN 33

#define NIGHT_THRESHOLD 2500
#define LONG_PRESS_TIME 800
#define DEBOUNCE_TIME 50

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;

// === BOOT ÜZENETEK ===
const char* bootMessages[] = {
  "Inicializálás",
  "BME szenzor ell",
  "LD szenzor ell",
  "Gravitacio kalib",
  "Pajzs diag",
  "Rendszer stabil"
};
const int messageCount = 6;

// === KÉPERNYŐK ===
enum Screen {
  SCREEN_LOGO,
  SCREEN_DASHBOARD,
  SCREEN_TEMP,
  SCREEN_HUMIDITY,
  SCREEN_PRESSURE,
  SCREEN_LIGHT,
  SCREEN_COUNT
};

// === MENÜ ===
enum MenuItem {
  MENU_LED_TOGGLE,
  MENU_NIGHT_TOGGLE,
  MENU_EXIT,
  MENU_COUNT
};

// === VÁLTOZÓK ===
float temp = 24.7;
float humidity = 50.0;
float pressure = 1013.0;
int ldrValue = 0;
int ldrSmoothed = 2048;

bool nightMode = false;
bool nightModeEnabled = true;
bool ledEnabled = true;
bool inMenu = false;

int currentScreen = SCREEN_DASHBOARD;
int menuSelection = 0;

// Gombok
bool btn1Pressed = false;
bool btn2Pressed = false;
unsigned long btn1PressStart = 0;
bool btn1WasLongPress = false;
unsigned long btn1LastChange = 0;
unsigned long btn2LastChange = 0;
bool btn1StableState = false;
bool btn2StableState = false;

unsigned long lastDashUpdate = 0;
unsigned long lastSensorRead = 0;
unsigned long lastFlash = 0;
int animFrame = 0;

// Fény history
int lightHistory[32];
int lightIndex = 0;

// === FÉNYERŐ ===
void setBrightness(uint8_t brightness) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(brightness);
}

// === BOOT FÜGGVÉNYEK ===

void typeText(const char* text, int x, int y, int delayMs = 30) {
  display.setCursor(x, y);
  for (int i = 0; text[i] != '\0'; i++) {
    display.print(text[i]);
    display.display();
    delay(delayMs);
  }
}

void animateProgress(const char* label, int duration) {
  int barWidth = 100;
  int barX = 14;
  int barY = 20;
  int steps = 50;
  int delayPerStep = duration / steps;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("> ");
  display.print(label);
  display.drawRect(barX, barY, barWidth, 8, SSD1306_WHITE);
  display.display();
  
  for (int i = 0; i <= steps; i++) {
    int fillWidth = map(i, 0, steps, 0, barWidth - 4);
    
    if (random(10) == 0 && i < steps - 5) {
      display.fillRect(barX + 2, barY + 2, fillWidth + random(-10, 10), 4, SSD1306_WHITE);
      display.display();
      delay(30);
    }
    
    display.fillRect(barX + 2, barY + 2, barWidth - 4, 4, SSD1306_BLACK);
    display.fillRect(barX + 2, barY + 2, fillWidth, 4, SSD1306_WHITE);
    
    display.fillRect(104, 10, 24, 8, SSD1306_BLACK);
    display.setCursor(104, 10);
    int pct = map(i, 0, steps, 0, 100);
    if (pct < 10) display.print(" ");
    if (pct < 100) display.print(" ");
    display.print(pct);
    display.print("%");
    
    display.display();
    delay(delayPerStep + random(-10, 20));
  }
  
  display.setCursor(0, 10);
  display.print("[OK]");
  display.display();
  delay(150);
}

void bootScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  
  for (int i = 0; i < 3; i++) {
    display.invertDisplay(true);
    delay(50);
    display.invertDisplay(false);
    delay(50);
  }
  
  display.drawLine(0, 0, 127, 0, SSD1306_WHITE);
  display.setCursor(8, 4);
  display.print("PHOTON-7 LANDER");
  display.drawLine(0, 14, 127, 14, SSD1306_WHITE);
  display.display();
  delay(500);
  
  typeText("Boot sequence...", 4, 20, 40);
  delay(500);
}

void scanEffect() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(8, 12);
  display.print("TERRAIN MAPPING");
  display.display();
  
  for (int y = 0; y < 32; y += 2) {
    display.drawLine(0, y, 127, y, SSD1306_WHITE);
    display.display();
    delay(20);
    display.drawLine(0, y, 127, y, SSD1306_BLACK);
  }
  
  display.setCursor(8, 12);
  display.print("TERRAIN MAPPING");
  
  for (int i = 0; i < 20; i++) {
    int x = random(10, 118);
    int y = random(4, 28);
    display.clearDisplay();
    display.drawRect(x - 3, y - 3, 6, 6, SSD1306_WHITE);
    display.display();
    delay(80);
  }
  delay(400);
}

void runBootSequence() {
  display.setRotation(0);
  bootScreen();
  delay(400);
  scanEffect();
  
  for (int i = 0; i < messageCount; i++) {
    int duration = random(500, 1200);
    animateProgress(bootMessages[i], duration);
  }
  
  display.clearDisplay();
  typeText("> System ONLINE", 0, 8, 20);
  typeText("> Dashboard AKTIV", 0, 20, 20);
  delay(600);
  
  for (int i = 0; i < 4; i++) {
    display.invertDisplay(true);
    delay(40);
    display.invertDisplay(false);
    delay(40);
  }
}

// === SEGÉDFÜGGVÉNYEK ===

void drawCenteredText(const char* text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((32 - w) / 2, y);
  display.print(text);
}

void drawMoon(int cx, int cy, int radius) {
  display.fillCircle(cx, cy, radius, SSD1306_WHITE);
  display.fillCircle(cx + 5, cy - 2, radius - 1, SSD1306_BLACK);
}

void updateLedFlash() {
  if (!ledEnabled) {
    analogWrite(LED_PIN, 0);
    return;
  }
  
  unsigned long now = millis();
  unsigned long elapsed = now - lastFlash;
  
  if (elapsed < 50) {
    analogWrite(LED_PIN, 255);
  } else if (elapsed < 150) {
    analogWrite(LED_PIN, 0);
  } else if (elapsed < 200) {
    analogWrite(LED_PIN, 255);
  } else {
    analogWrite(LED_PIN, 0);
    if (elapsed > 2000) {
      lastFlash = now;
    }
  }
}

void updateLightHistory() {
  lightHistory[lightIndex] = ldrSmoothed;
  lightIndex = (lightIndex + 1) % 32;
}

// === SZENZOR OLVASÁS ===
void readSensors() {
  temp = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  
  int rawLdr = analogRead(LDR_PIN);
  ldrSmoothed = (ldrSmoothed * 7 + rawLdr) / 8;
  ldrValue = ldrSmoothed;
  
  if (nightModeEnabled) {
    nightMode = (ldrSmoothed < NIGHT_THRESHOLD);
  } else {
    nightMode = false;
  }
  
  updateLightHistory();
}

// === GOMBOK KEZELÉSE ===
void handleButtons() {
  unsigned long now = millis();
  
  // BTN1 olvasás debounce-szal
  bool btn1Raw = digitalRead(BTN1_PIN) == LOW;
  if (btn1Raw != btn1StableState && (now - btn1LastChange > DEBOUNCE_TIME)) {
    btn1LastChange = now;
    btn1StableState = btn1Raw;
    
    if (btn1StableState) {
      // Lenyomás
      btn1PressStart = now;
      btn1WasLongPress = false;
    } else {
      // Elengedés
      if (!btn1WasLongPress) {
        if (inMenu) {
          menuSelection = (menuSelection + 1) % MENU_COUNT;
        } else {
          currentScreen--;
          if (currentScreen < 0) currentScreen = SCREEN_COUNT - 1;
        }
      }
    }
  }
  
  // BTN1 hosszan nyomva tartás
  if (btn1StableState && !btn1WasLongPress && (now - btn1PressStart > LONG_PRESS_TIME)) {
    btn1WasLongPress = true;
    if (!inMenu) {
      inMenu = true;
      menuSelection = 0;
    }
  }
  
  // BTN2 olvasás debounce-szal
  bool btn2Raw = digitalRead(BTN2_PIN) == LOW;
  if (btn2Raw != btn2StableState && (now - btn2LastChange > DEBOUNCE_TIME)) {
    btn2LastChange = now;
    btn2StableState = btn2Raw;
    
    if (!btn2StableState && btn2Pressed) {
      // Elengedés
      if (inMenu) {
        switch (menuSelection) {
          case MENU_LED_TOGGLE:
            ledEnabled = !ledEnabled;
            break;
          case MENU_NIGHT_TOGGLE:
            nightModeEnabled = !nightModeEnabled;
            break;
          case MENU_EXIT:
            inMenu = false;
            break;
        }
      } else {
        currentScreen = (currentScreen + 1) % SCREEN_COUNT;
      }
    }
  }
  
  btn1Pressed = btn1StableState;
  btn2Pressed = btn2StableState;
}

// === MENÜ RAJZOLÁS ===
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setCursor(2, 2);
  display.print("MENU");
  
  int startY = 20;
  
  // LED
  int y = startY;
  if (menuSelection == 0) {
    display.fillRect(0, y - 2, 32, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setCursor(2, y);
  display.print("LED");
  display.setCursor(2, y + 14);
  display.setTextColor(SSD1306_WHITE);
  display.print(ledEnabled ? "[BE]" : "[KI]");
  
  // Éjszakai
  y = startY + 32;
  if (menuSelection == 1) {
    display.fillRect(0, y - 2, 32, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setCursor(2, y);
  display.print("EJJ");
  display.setCursor(2, y + 14);
  display.setTextColor(SSD1306_WHITE);
  display.print(nightModeEnabled ? "[BE]" : "[KI]");
  
  // Vissza
  y = startY + 64;
  if (menuSelection == 2) {
    display.fillRect(0, y - 2, 32, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setCursor(2, y);
  display.print("VISSZA");
  display.setTextColor(SSD1306_WHITE);
  
  display.display();
}

// === KÉPERNYŐK ===

void drawLightGraph(int startY, int height) {
  int minVal = lightHistory[0];
  int maxVal = lightHistory[0];
  for (int i = 1; i < 32; i++) {
    if (lightHistory[i] < minVal) minVal = lightHistory[i];
    if (lightHistory[i] > maxVal) maxVal = lightHistory[i];
  }
  
  if (maxVal - minVal < 100) {
    int mid = (minVal + maxVal) / 2;
    minVal = mid - 50;
    maxVal = mid + 50;
  }
  
  for (int i = 1; i < 30; i++) {
    int idx = (lightIndex + i) % 32;
    int prevIdx = (lightIndex + i - 1) % 32;
    
    int y1 = map(lightHistory[prevIdx], minVal, maxVal, startY + height - 2, startY + 2);
    int y2 = map(lightHistory[idx], minVal, maxVal, startY + height - 2, startY + 2);
    
    y1 = constrain(y1, startY, startY + height);
    y2 = constrain(y2, startY, startY + height);
    
    display.drawLine(i - 1, y1, i, y2, SSD1306_WHITE);
  }
}

void drawRadar(int cx, int cy, int radius) {
  display.drawCircle(cx, cy, radius, SSD1306_WHITE);
  
  float angle = (animFrame % 60) * 6 * PI / 180.0;
  int x = cx + cos(angle) * radius;
  int y = cy + sin(angle) * radius;
  display.drawLine(cx, cy, x, y, SSD1306_WHITE);
  
  for (int i = 1; i < 4; i++) {
    float prevAngle = ((animFrame - i * 3) % 60) * 6 * PI / 180.0;
    int px = cx + cos(prevAngle) * (radius - i);
    int py = cy + sin(prevAngle) * (radius - i);
    display.drawPixel(px, py, SSD1306_WHITE);
  }
}

void drawScreenDashboard() {
  display.clearDisplay();
  display.setTextSize(1);
  
  drawCenteredText("P-7", 2);
  
  unsigned long elapsed = millis() - lastFlash;
  int dotSize = (ledEnabled && (elapsed < 50 || (elapsed >= 150 && elapsed < 200))) ? 3 : 1;
  display.fillCircle(28, 5, dotSize, SSD1306_WHITE);
  
  drawLightGraph(14, 24);
  drawRadar(16, 52, 10);
  
  char tempStr[10];
  sprintf(tempStr, "%d%cC", (int)temp, 247);
  drawCenteredText(tempStr, 68);
  
  char humStr[10];
  sprintf(humStr, "%d%%", (int)humidity);
  drawCenteredText(humStr, 86);
  
  char pressStr[10];
  sprintf(pressStr, "%d", (int)pressure);
  drawCenteredText(pressStr, 104);
  drawCenteredText("hPa", 114);
  
  display.display();
}

void drawScreenTemp() {
  display.clearDisplay();
  
  // Hőmérő ikon
  display.drawCircle(16, 30, 8, SSD1306_WHITE);
  display.fillCircle(16, 30, 5, SSD1306_WHITE);
  display.drawRect(13, 10, 7, 20, SSD1306_WHITE);
  int fillHeight = map(constrain(temp, 10, 40), 10, 40, 0, 16);
  display.fillRect(14, 26 - fillHeight, 5, fillHeight + 4, SSD1306_WHITE);
  
  // Érték + °C középre
  display.setTextSize(1);
  char tempStr[10];
  sprintf(tempStr, "%d%cC", (int)temp, 247);
  drawCenteredText(tempStr, 60);
  
  // Skála
  display.drawRect(4, 85, 24, 6, SSD1306_WHITE);
  int posX = map(constrain(temp, 15, 35), 15, 35, 5, 26);
  display.fillRect(posX - 1, 86, 3, 4, SSD1306_WHITE);
  display.setCursor(0, 95);
  display.print("15");
  display.setCursor(18, 95);
  display.print("35");
  
  display.display();
}

void drawScreenHumidity() {
  display.clearDisplay();
  
  // Vízcsepp ikon
  display.fillTriangle(16, 10, 8, 28, 24, 28, SSD1306_WHITE);
  display.fillCircle(16, 30, 8, SSD1306_WHITE);
  
  // Érték + % középre (kisebb méret)
  display.setTextSize(1);
  char humStr[10];
  sprintf(humStr, "%d%%", (int)humidity);
  drawCenteredText(humStr, 55);
  
  // Progress bar
  display.drawRect(2, 75, 28, 8, SSD1306_WHITE);
  int fillW = map(constrain(humidity, 0, 100), 0, 100, 0, 24);
  display.fillRect(4, 77, fillW, 4, SSD1306_WHITE);
  
  display.display();
}

void drawScreenPressure() {
  display.clearDisplay();
  
  // Barométer ikon
  display.drawCircle(16, 25, 12, SSD1306_WHITE);
  display.drawCircle(16, 25, 10, SSD1306_WHITE);
  float angle = map(constrain(pressure, 980, 1040), 980, 1040, 225, -45) * PI / 180.0;
  int nx = 16 + cos(angle) * 8;
  int ny = 25 - sin(angle) * 8;
  display.drawLine(16, 25, nx, ny, SSD1306_WHITE);
  
  // Érték
  display.setTextSize(1);
  char pressStr[10];
  sprintf(pressStr, "%d", (int)pressure);
  drawCenteredText(pressStr, 55);
  drawCenteredText("hPa", 68);
  
  // Skála
  display.setCursor(0, 90);
  display.print("L");
  display.setCursor(13, 90);
  display.print("N");
  display.setCursor(26, 90);
  display.print("H");
  
  display.display();
}

void drawScreenLight() {
  display.clearDisplay();
  
  // Nap ikon
  display.fillCircle(16, 25, 6, SSD1306_WHITE);
  for (int i = 0; i < 8; i++) {
    float angle = i * 45 * PI / 180.0;
    int x1 = 16 + cos(angle) * 9;
    int y1 = 25 + sin(angle) * 9;
    int x2 = 16 + cos(angle) * 12;
    int y2 = 25 + sin(angle) * 12;
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
  
  // Érték középre (kisebb méret)
  display.setTextSize(1);
  char ldrStr[10];
  sprintf(ldrStr, "%d", ldrSmoothed);
  drawCenteredText(ldrStr, 55);
  
  // Grafikon
  drawLightGraph(75, 35);
  
  display.display();
}

void drawScreenLogo() {
  display.clearDisplay();
  
  // PHOTON 7 - minden 2-es méretben, középre igazítva, függőlegesen
  display.setTextSize(2);
  
  const char* letters = "PHOTON7";
  int y = 2;
  for (int i = 0; letters[i] != '\0'; i++) {
    // Középre igazítás: 2-es méret = 12px széles karakter, képernyő 32px
    display.setCursor(10, y);
    display.print(letters[i]);
    y += 18;  // Nagyobb távolság, ne érjenek össze
  }
  
  // Futó fények bal és jobb oldalon - ellentétes irányba
  int leftPos = (animFrame * 3) % 128;          // Lefelé fut
  int rightPos = 127 - ((animFrame * 3) % 128); // Felfelé fut
  
  // Bal oldali fénycsík (lefelé) - hosszabb csóva
  for (int i = 0; i < 20; i++) {
    int yy = leftPos - i;
    if (yy >= 0 && yy < 128) {
      display.drawPixel(1, yy, SSD1306_WHITE);
      if (i < 12) display.drawPixel(2, yy, SSD1306_WHITE);
      if (i < 6) display.drawPixel(0, yy, SSD1306_WHITE);
    }
  }
  
  // Jobb oldali fénycsík (felfelé) - hosszabb csóva
  for (int i = 0; i < 20; i++) {
    int yy = rightPos + i;
    if (yy >= 0 && yy < 128) {
      display.drawPixel(30, yy, SSD1306_WHITE);
      if (i < 12) display.drawPixel(29, yy, SSD1306_WHITE);
      if (i < 6) display.drawPixel(31, yy, SSD1306_WHITE);
    }
  }
  
  display.setTextSize(1);
  display.display();
}

void drawNightScreen() {
  display.clearDisplay();
  
  drawMoon(16, 50, 10);
  
  if (animFrame % 20 < 15) display.drawPixel(5, 25, SSD1306_WHITE);
  if (animFrame % 25 < 18) display.drawPixel(28, 30, SSD1306_WHITE);
  if (animFrame % 18 < 12) display.drawPixel(8, 80, SSD1306_WHITE);
  if (animFrame % 22 < 16) display.drawPixel(25, 95, SSD1306_WHITE);
  if (animFrame % 30 < 20) display.drawPixel(3, 110, SSD1306_WHITE);
  if (animFrame % 15 < 10) display.drawPixel(28, 115, SSD1306_WHITE);
  
  display.display();
}

// === FŐ FRISSÍTÉS ===
void updateDisplay() {
  animFrame++;
  
  updateLedFlash();
  
  if (millis() - lastSensorRead > 150) {
    lastSensorRead = millis();
    readSensors();
  }
  
  if (nightMode && !inMenu) {
    setBrightness(0);
    display.dim(true);
    drawNightScreen();
    return;
  }
  
  setBrightness(128);
  display.dim(false);
  
  if (inMenu) {
    drawMenu();
    return;
  }
  
  switch (currentScreen) {
    case SCREEN_LOGO:
      drawScreenLogo();
      break;
    case SCREEN_DASHBOARD:
      drawScreenDashboard();
      break;
    case SCREEN_TEMP:
      drawScreenTemp();
      break;
    case SCREEN_HUMIDITY:
      drawScreenHumidity();
      break;
    case SCREEN_PRESSURE:
      drawScreenPressure();
      break;
    case SCREEN_LIGHT:
      drawScreenLight();
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== PHOTON-7 LANDER ===");
  
    // Beépített LED kikapcsolása

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);


  randomSeed(analogRead(0));
  
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  
  ldrSmoothed = analogRead(LDR_PIN);
  
  for (int i = 0; i < 32; i++) {
    lightHistory[i] = ldrSmoothed;
  }
  
  if (bme.begin(0x76)) {
    Serial.println("BME280 at 0x76");
  } else if (bme.begin(0x77)) {
    Serial.println("BME280 at 0x77");
  } else {
    Serial.println("BME280 NOT FOUND!");
  }
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED failed!");
    while (true);
  }
  
  setBrightness(0);
  display.dim(true);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
  delay(200);
  
  readSensors();
  runBootSequence();
  
  display.setRotation(1);
  display.clearDisplay();
  
  lastFlash = millis();

  digitalWrite(2, LOW);
}

void loop() {
  handleButtons();
  
  if (millis() - lastDashUpdate > 100) {
    lastDashUpdate = millis();
    updateDisplay();
  }
  
  updateLedFlash();
}