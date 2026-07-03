/* COMPLETE SYSTEM: FINDER + FACES + EMOTIONS (DEMO MODE + BACK BUTTON + FIX)
   - FIXED: Green Circle Glitch (Moved circle down to fit in clear area)
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <MD_MAX72xx.h>
#include <Arduino.h>
#include <time.h>

// ==========================================
//           CONFIGURATION: MATRIX
// ==========================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW 
#define MAX_DEVICES 1

#define MATRIX_DIN  19
#define MATRIX_CS   20
#define MATRIX_CLK  21

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, MATRIX_DIN, MATRIX_CLK, MATRIX_CS, MAX_DEVICES);

// --- FACE BITMAPS ---
const uint8_t idleFace[8] = {
  0x00, 0x24, 0x24, 0x24, 0x00, 0x00, 0x3C, 0x00
};

const uint8_t smileFace[8] = {
  0x00, 0x24, 0x24, 0x00, 0x42, 0x3C, 0x00, 0x00
};

// ==========================================
//           CONFIGURATION: FINDER
// ==========================================
#define TFT_CS    10
#define TFT_RST   2
#define TFT_DC    1
#define TFT_MOSI  11
#define TFT_SCLK  12

#define LED_RED     4
#define LED_GREEN   5
#define LED_YELLOW  6 
#define BTN_NEXT    7
#define BTN_SELECT  15

int THRESHOLD_YELLOW = -40; 
int THRESHOLD_RED    = -60; 

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
BLEScan* pBLEScan;

enum AppState { 
  MAIN_MENU, 
  ITEM_SELECT, 
  TRACKING_MODE, 
  FEELING_SELECT, 
  FEELING_SCALE, 
  FEELING_SELECT_2,
  FEELING_SCALE_2,
  RECOMMENDATION_RESULT,
  TRACKING_GIFT_MODE
};
AppState currentState = MAIN_MENU;

String mainOptions[] = {"Find Items", "Feelings"};
int mainIndex = 0;
String itemOptions[] = {"TAG_KEYS", "TAG_WALLET"}; 
int itemIndex = 0;
String targetName = "";

// ==========================================
//     EMOTION RECOMMENDATION SYSTEM
// ==========================================
#define MAX_STUFF 20

int oldStuffCount = 0;

struct Stuff {
  int index;
  String name;
  String tags[3];   
  int scores[3];    
  time_t lastUsed;  
};

Stuff oldStuffs[MAX_STUFF];

String emotionsList[10] = {
  "Angry", "Sad", "Frustrated", "Ashamed", "Lonely", 
  "Anxious", "Bored", "Disappointed", "Confused", "Uncomfortable"
};

struct NegPosMap {
  String neg;
  String pos;
};

NegPosMap mapping[] = {
  {"Angry","Peaceful"},
  {"Sad","Happy"},
  {"Frustrated","Proud"},
  {"Ashamed","Hilarious"},
  {"Lonely","Loved"},
  {"Anxious","Secure"},
  {"Bored","Playful"},
  {"Disappointed","Confident"},
  {"Confused","Cozy"},
  {"Uncomfortable","Relax"}
};

// Emotion selection state
int feelingIndex = 0;
String selectedFeeling1 = "";
int intensity1 = 1;
String selectedFeeling2 = "";
int intensity2 = 1;
String recommendedItem = "";

// ==========================================
//        EMOTION ALGORITHM FUNCTIONS
// ==========================================
void insertOldStuff(String tag1, int score1, String tag2, int score2, String tag3, int score3) {
  if (oldStuffCount >= MAX_STUFF) return;
  
  int currentID = oldStuffCount + 1;
  String autoName = "ITEM_" + String(currentID);

  oldStuffs[oldStuffCount].index = currentID;
  oldStuffs[oldStuffCount].name = autoName; 
  oldStuffs[oldStuffCount].tags[0] = tag1;
  oldStuffs[oldStuffCount].tags[1] = tag2;
  oldStuffs[oldStuffCount].tags[2] = tag3;
  oldStuffs[oldStuffCount].scores[0] = score1;
  oldStuffs[oldStuffCount].scores[1] = score2;
  oldStuffs[oldStuffCount].scores[2] = score3;
  oldStuffs[oldStuffCount].lastUsed = time(NULL);

  oldStuffCount++;
}

String getPositiveTag(String neg){
  for(int i=0; i<10; i++){
    if(mapping[i].neg == neg) return mapping[i].pos;
  }
  return "";
}

float computeScore(Stuff s, String neg1, int score1, String neg2, int score2){
  float totalScore = 0;
  String pos1 = getPositiveTag(neg1);
  String pos2 = getPositiveTag(neg2);
  
  int s1=0, s2=0; 
  for(int i=0; i<3; i++){
    if(s.tags[i] == pos1) s1 = s.scores[i];
    if(s.tags[i] == pos2) s2 = s.scores[i];
  }
  
  totalScore = (score1 * s1/10.0) + (score2 * s2/10.0);
  return totalScore;
}

struct ScoredStuff {
  int idx;
  float score;
  time_t lastUsed;
};

String findTop3OldStuffs(String neg1, int score1, String neg2, int score2){
  if (oldStuffCount == 0) return "NONE";

  ScoredStuff scored[oldStuffCount];
  for(int i=0; i<oldStuffCount; i++){
    scored[i].idx = i;
    scored[i].score = computeScore(oldStuffs[i], neg1, score1, neg2, score2);
    scored[i].lastUsed = oldStuffs[i].lastUsed;
  }

  for(int i=0; i < oldStuffCount - 1; i++){
    for(int j=i+1; j < oldStuffCount; j++){
      if(scored[j].score > scored[i].score || 
         (scored[j].score == scored[i].score && scored[j].lastUsed < scored[i].lastUsed)){
        ScoredStuff tmp = scored[i];
        scored[i] = scored[j];
        scored[j] = tmp;
      }
    }
  }

  int bestPos = 0; 
  for (int i = 1; i < 3 && i < oldStuffCount; i++) {
      if (scored[i].lastUsed < scored[bestPos].lastUsed) {
          bestPos = i;
      }
  }
  
  int idx = scored[bestPos].idx;
  oldStuffs[idx].lastUsed = time(NULL);
  
  return oldStuffs[idx].name;
}

// ==========================================
//                SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. Setup Finder Hardware
  pinMode(LED_RED, OUTPUT); 
  pinMode(LED_GREEN, OUTPUT); 
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(BTN_NEXT, INPUT_PULLUP); 
  pinMode(BTN_SELECT, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);
  
  // 2. Setup BLE
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // 3. Setup Matrix
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 5); 
  mx.clear();
  drawFace(idleFace);

  // 4. Initialize Emotion Database
  insertOldStuff("Peaceful", 9, "Happy", 7, "Relax", 8);   
  insertOldStuff("Proud", 10, "Confident", 8, "Secure", 6); 
  insertOldStuff("Playful", 9, "Loved", 9, "Cozy", 7);
  insertOldStuff("Hilarious", 10, "Happy", 8, "Playful", 7); 
  insertOldStuff("Secure", 9, "Relax", 9, "Cozy", 8);       
  insertOldStuff("Confident", 9, "Proud", 7, "Peaceful", 6); 
  insertOldStuff("Loved", 10, "Cozy", 9, "Happy", 8);      
  insertOldStuff("Relax", 10, "Peaceful", 9, "Cozy", 7); 
  insertOldStuff("Playful", 10, "Hilarious", 9, "Happy", 7); 
  insertOldStuff("Cozy", 10, "Secure", 8, "Relax", 9);

  drawMainMenu();
}

// ==========================================
//              MAIN LOOP
// ==========================================
void loop() {
  // --- NEXT BUTTON (Cycle options) ---
  if (digitalRead(BTN_NEXT) == LOW) { 
    handleNextButton(); 
    delay(200); 
  }

  // --- SELECT BUTTON (Short = Select, Long = Back) ---
  if (digitalRead(BTN_SELECT) == LOW) {
    unsigned long pressStart = millis();
    
    // Wait until button is released
    while(digitalRead(BTN_SELECT) == LOW); 
    
    unsigned long pressDuration = millis() - pressStart;

    if (pressDuration > 800) { 
      // LONG PRESS (> 800ms) -> BACK ACTION
      handleBackButton();
    } else {
      // SHORT PRESS -> SELECT ACTION
      handleSelectButton();
    }
    delay(200); 
  }

  // --- STATE MACHINES ---
  if (currentState == TRACKING_MODE) {
    runTrackingLogic();
  }
  
  if (currentState == TRACKING_GIFT_MODE) {
    runGiftTrackingLogic();
  }
}

// ==========================================
//          BUTTON HANDLERS
// ==========================================

void handleBackButton() {
  setLED(1,1,0); delay(100); setLED(0,0,0); 

  switch(currentState) {
    case ITEM_SELECT:
      currentState = MAIN_MENU;
      drawMainMenu();
      break;
      
    case TRACKING_MODE:
      currentState = ITEM_SELECT;
      drawFace(idleFace);
      drawItemMenu();
      break;

    case FEELING_SELECT:
      currentState = MAIN_MENU;
      drawMainMenu();
      break;

    case FEELING_SCALE:
      currentState = FEELING_SELECT;
      drawFeelingMenu();
      break;

    case FEELING_SELECT_2:
      currentState = FEELING_SCALE;
      drawScaleScreen(1);
      break;

    case FEELING_SCALE_2:
      currentState = FEELING_SELECT_2;
      drawFeelingMenu2();
      break;

    case RECOMMENDATION_RESULT:
      currentState = FEELING_SCALE_2;
      drawScaleScreen(2);
      break;

    case TRACKING_GIFT_MODE:
      currentState = RECOMMENDATION_RESULT;
      drawFace(idleFace);
      drawRecommendationScreen();
      break;
      
    default:
      break;
  }
}

void handleNextButton() {
  switch(currentState) {
    case MAIN_MENU: 
      mainIndex = (mainIndex + 1) % 2; 
      drawMainMenu(); 
      break;
    case ITEM_SELECT: 
      itemIndex = (itemIndex + 1) % 2; 
      drawItemMenu(); 
      break;
    case FEELING_SELECT: 
      feelingIndex = (feelingIndex + 1) % 10; 
      drawFeelingMenu(); 
      break;
    case FEELING_SCALE: 
      intensity1++; 
      if(intensity1 > 10) intensity1 = 1; 
      drawScaleScreen(1); 
      break;
    case FEELING_SELECT_2: 
      feelingIndex = (feelingIndex + 1) % 10; 
      drawFeelingMenu2(); 
      break;
    case FEELING_SCALE_2: 
      intensity2++; 
      if(intensity2 > 10) intensity2 = 1; 
      drawScaleScreen(2); 
      break;
  }
}

void handleSelectButton() {
  switch(currentState) {
    case MAIN_MENU:
      if(mainIndex == 0) { 
        currentState = ITEM_SELECT; 
        drawItemMenu(); 
      } else { 
        currentState = FEELING_SELECT; 
        feelingIndex = 0;
        drawFeelingMenu(); 
      }
      break;
      
    case ITEM_SELECT:
      targetName = itemOptions[itemIndex];
      currentState = TRACKING_MODE;
      
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 10); 
      tft.setTextColor(ST7735_WHITE); 
      tft.setTextSize(1);
      tft.print("Finding: "); 
      tft.println(targetName.substring(4));
      break;
      
    case TRACKING_MODE:
      setLED(0,0,0);
      currentState = MAIN_MENU;
      drawFace(idleFace);
      drawMainMenu();
      break;
      
    case FEELING_SELECT:
      selectedFeeling1 = emotionsList[feelingIndex];
      currentState = FEELING_SCALE; 
      intensity1 = 5; 
      drawScaleScreen(1); 
      break;
      
    case FEELING_SCALE:
      currentState = FEELING_SELECT_2; 
      feelingIndex = 0;
      drawFeelingMenu2(); 
      break;
      
    case FEELING_SELECT_2:
      selectedFeeling2 = emotionsList[feelingIndex];
      currentState = FEELING_SCALE_2; 
      intensity2 = 5; 
      drawScaleScreen(2); 
      break;
      
    case FEELING_SCALE_2:
      recommendedItem = findTop3OldStuffs(selectedFeeling1, intensity1, selectedFeeling2, intensity2);
      currentState = RECOMMENDATION_RESULT; 
      drawRecommendationScreen(); 
      break;
      
    case RECOMMENDATION_RESULT:
      targetName = recommendedItem;
      currentState = TRACKING_GIFT_MODE;
      
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 10); 
      tft.setTextColor(ST7735_WHITE); 
      tft.setTextSize(1);
      tft.print("Finding: "); 
      tft.println(recommendedItem);
      break;
      
    case TRACKING_GIFT_MODE:
      setLED(0,0,0);
      currentState = MAIN_MENU;
      drawFace(idleFace);
      drawMainMenu();
      break;
  }
}

// ==========================================
//          HELPER: DRAW FACE
// ==========================================
void drawFace(const uint8_t *faceArray) {
  mx.clear();
  for (int i = 0; i < 8; i++) {
    mx.setRow(0, i, faceArray[i]);
  }
}

// ==========================================
//          TRACKING LOGIC (DEMO MODE)
// ==========================================
void runTrackingLogic() {
  BLEScanResults *foundDevices = pBLEScan->start(1, false);
  bool found = false;
  int currentRSSI = -100;

  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    // DEMO HACK: We IGNORE targetName and ALWAYS look for TAG_KEYS
    if (String(device.getName().c_str()).indexOf("TAG_KEYS") >= 0) {
      currentRSSI = device.getRSSI();
      found = true;
    }
  }
  pBLEScan->clearResults();
  updateSearchUI(found, currentRSSI);
}

void updateSearchUI(bool found, int rssi) {
  tft.fillRect(0, 30, 160, 128, ST7735_BLACK);

  if (!found) {
    setLED(0, 0, 1); // Blue
    drawFace(idleFace);
    tft.setCursor(20, 60); 
    tft.setTextColor(ST7735_BLUE); 
    tft.setTextSize(2);
    tft.println("SCANNING...");
    return;
  }

  uint16_t screenColor;
  String statusText;
  int circleSize;

  if (rssi < THRESHOLD_RED) { 
    setLED(1, 0, 0); 
    screenColor = ST7735_RED; 
    statusText = "FAR"; 
    circleSize = 15;
    drawFace(idleFace);
  } 
  else if (rssi < THRESHOLD_YELLOW) {
    setLED(0, 0, 1); 
    screenColor = ST7735_YELLOW; 
    statusText = "CLOSER"; 
    circleSize = 35;
    drawFace(idleFace);
  } 
  else {
    setLED(0, 1, 0); 
    screenColor = ST7735_GREEN; 
    statusText = "HERE!"; 
    circleSize = 50;
    drawFace(smileFace); // FOUND IT!
  }

  // FIX: Moved circle down to 90 to avoid drawing over the uncleared header
  tft.fillCircle(80, 90, circleSize, screenColor);
  
  tft.setCursor(10, 110); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(2); 
  tft.println(statusText);
}

void runGiftTrackingLogic() {
  BLEScanResults *foundDevices = pBLEScan->start(1, false);
  bool found = false;
  int currentRSSI = -100;

  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    // DEMO HACK: ALWAYS look for TAG_KEYS for the gift too
    if (String(device.getName().c_str()).indexOf("TAG_KEYS") >= 0) {
      currentRSSI = device.getRSSI();
      found = true;
    }
  }
  pBLEScan->clearResults();
  updateGiftSearchUI(found, currentRSSI);
}

void updateGiftSearchUI(bool found, int rssi) {
  tft.fillRect(0, 30, 160, 128, ST7735_BLACK);

  if (!found) {
    setLED(0, 0, 1); 
    drawFace(idleFace); 
    tft.setCursor(20, 60); 
    tft.setTextColor(ST7735_BLUE); 
    tft.setTextSize(2);
    tft.println("SCANNING...");
    return;
  }

  uint16_t screenColor;
  String statusText;
  int circleSize;

  if (rssi < THRESHOLD_RED) { 
    setLED(1, 0, 0); 
    screenColor = ST7735_RED; 
    statusText = "FAR"; 
    circleSize = 15;
    drawFace(idleFace);
  } 
  else if (rssi < THRESHOLD_YELLOW) {
    setLED(0, 0, 1); 
    screenColor = ST7735_YELLOW; 
    statusText = "CLOSER"; 
    circleSize = 35;
    drawFace(idleFace);
  } 
  else {
    setLED(0, 1, 0); 
    screenColor = ST7735_GREEN; 
    statusText = "FOUND!"; 
    circleSize = 50;
    drawFace(smileFace);
  }

  // FIX: Moved circle down to 90 to avoid drawing over the uncleared header
  tft.fillCircle(80, 90, circleSize, screenColor);

  tft.setCursor(10, 110); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(2); 
  tft.println(statusText);
}

// ==========================================
//          MENU DRAWING
// ==========================================
void drawMainMenu() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 10); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(2); 
  tft.println("MAIN MENU");
  
  for (int i = 0; i < 2; i++) {
    if (i == mainIndex) tft.setTextColor(ST7735_YELLOW); 
    else tft.setTextColor(0x7BEF);
    tft.setCursor(10, 50 + (i*30)); 
    if (i == mainIndex) tft.print("> "); 
    tft.println(mainOptions[i]);
  }
}

void drawItemMenu() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 10); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(2); 
  tft.println("FIND ITEM");
  
  for (int i = 0; i < 2; i++) {
    if (i == itemIndex) tft.setTextColor(ST7735_YELLOW); 
    else tft.setTextColor(0x7BEF);
    tft.setCursor(10, 50 + (i*30)); 
    if (i == itemIndex) tft.print("> "); 
    tft.println(itemOptions[i].substring(4));
  }
}

void drawFeelingMenu() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 10); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(1); 
  tft.println("I FEEL... (1/2)");
  
  int startIdx = (feelingIndex / 5) * 5;
  for (int i = 0; i < 5 && (startIdx + i) < 10; i++) {
    int idx = startIdx + i;
    if (idx == feelingIndex) tft.setTextColor(ST7735_CYAN); 
    else tft.setTextColor(0x7BEF);
    tft.setCursor(10, 35 + (i*20)); 
    if (idx == feelingIndex) tft.print("> "); 
    tft.println(emotionsList[idx]);
  }
}

void drawFeelingMenu2() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 10); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(1); 
  tft.println("I ALSO FEEL... (2/2)");
  
  int startIdx = (feelingIndex / 5) * 5;
  for (int i = 0; i < 5 && (startIdx + i) < 10; i++) {
    int idx = startIdx + i;
    if (idx == feelingIndex) tft.setTextColor(ST7735_CYAN); 
    else tft.setTextColor(0x7BEF);
    tft.setCursor(10, 35 + (i*20)); 
    if (idx == feelingIndex) tft.print("> "); 
    tft.println(emotionsList[idx]);
  }
}

void drawScaleScreen(int emotionNum) {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 10); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(1); 
  tft.print("INTENSITY ");
  tft.print(emotionNum);
  tft.print("/2");
  
  tft.setCursor(10, 30); 
  tft.setTextColor(ST7735_CYAN); 
  tft.setTextSize(1);
  if(emotionNum == 1) tft.println(selectedFeeling1);
  else tft.println(selectedFeeling2);
  
  tft.setCursor(60, 50); 
  tft.setTextColor(ST7735_YELLOW); 
  tft.setTextSize(5); 
  if(emotionNum == 1) tft.println(intensity1);
  else tft.println(intensity2);
  
  int w;
  if(emotionNum == 1) w = map(intensity1, 1, 10, 10, 140);
  else w = map(intensity2, 1, 10, 10, 140);
  
  tft.fillRect(10, 100, w, 10, ST7735_GREEN); 
  tft.drawRect(10, 100, 140, 10, ST7735_WHITE);
}

void drawRecommendationScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  // Show smile face when gift appears!
  drawFace(smileFace);
  
  tft.setCursor(10, 10); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(1);
  tft.println("RECOMMENDED:");
  
  
  tft.setCursor(53, 58); 
  tft.setTextColor(ST7735_WHITE); 
  tft.setTextSize(2); 
  tft.println(recommendedItem);
  
  tft.setCursor(10, 100); 
  tft.setTextColor(ST7735_CYAN); 
  tft.setTextSize(1);
  tft.print(selectedFeeling1);
  tft.print(" ");
  tft.print(intensity1);
  tft.print(" + ");
  tft.print(selectedFeeling2);
  tft.print(" ");
  tft.println(intensity2);
  
  tft.setCursor(10, 115); 
  tft.setTextColor(ST7735_YELLOW); 
  tft.setTextSize(1);
  tft.println("Press SELECT to find!");
  
  setLED(0,1,0); 
  delay(200); 
  setLED(0,0,0); 
  delay(200); 
  setLED(0,1,0);
}

void setLED(int r, int g, int b) { 
  digitalWrite(LED_RED, r); 
  digitalWrite(LED_GREEN, g); 
  digitalWrite(LED_YELLOW, b); 
}