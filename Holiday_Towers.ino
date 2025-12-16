#include <Adafruit_NeoPixel.h>

#define PIN 2
#define NUM_LEDS 256
#define WIDTH 32
#define HEIGHT 8

Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

// Maximum rings supported
#define MAX_RINGS 5

// Current number of rings (configurable via n=3, n=4, n=5 commands)
int numRings = 5;

// Ring widths for all 5 possible rings
int ringWidths[MAX_RINGS] = {1, 3, 5, 7, 9};

uint32_t ringPatterns[MAX_RINGS][9];

uint32_t christmasColors[4];
uint32_t green;
uint32_t brown;
uint32_t snow;
uint32_t frozenGreen;
uint32_t frozenBrown;

int treeCenters[3] = {4, 15, 26};

// Game state
int pegs[3][MAX_RINGS];
int pegHeights[3];

// ============ DEBUG CONFIGURATION ============
bool winGameImmediately = false;

// ============ SNOW TIMER CONFIGURATION ============
bool snowTimerMode = true;
int snowSpawnInterval = 500;
int snowFallSpeed = 150;
int snowSlideChance = 40;

// Blizzard mode - snow gets faster each win
bool blizzardMode = false;
int blizzardLevel = 1;
int blizzardStartInterval = 2000;  // Level 1 starts here
int blizzardMinInterval = 50;      // fastest possible spawn rate
bool blizzardWaitingForMove = false;  // Wait for first move before snow starts

// Level display state
bool showingLevelDisplay = false;
unsigned long levelDisplayStart = 0;
int levelDisplayDuration = 2500;  // Show level for 2.5 seconds

// Calculate snow interval for a given blizzard level
int getBlizzardInterval(int level) {
  // Levels 1-11: 2000 -> 1000 (decrease by 100)
  // Levels 12+: 1000 -> 50 (decrease by 50)
  if (level <= 11) {
    return 2000 - (level - 1) * 100;  // 2000, 1900, 1800... 1000
  } else {
    int interval = 1000 - (level - 11) * 50;  // 950, 900, 850... 50
    if (interval < blizzardMinInterval) interval = blizzardMinInterval;
    return interval;
  }
}

// Intro snow state
#define MAX_INTRO_SNOWFLAKES 10
int introSnowX[MAX_INTRO_SNOWFLAKES];
int introSnowY[MAX_INTRO_SNOWFLAKES];
bool introSnowActive[MAX_INTRO_SNOWFLAKES];
int introSnowStartX[MAX_INTRO_SNOWFLAKES];

// Timer snow state (only for n=5)
#define MAX_TIMER_SNOWFLAKES 20
int timerSnowX[MAX_TIMER_SNOWFLAKES];
int timerSnowY[MAX_TIMER_SNOWFLAKES];
bool timerSnowActive[MAX_TIMER_SNOWFLAKES];
int timerSnowStartX[MAX_TIMER_SNOWFLAKES];

int snowAccumulation[WIDTH];
bool snowExcluded[WIDTH];

bool gameStarted = false;
unsigned long lastSnowUpdate = 0;
unsigned long lastSnowSpawn = 0;
unsigned long snowStartTime = 0;
int snowSpeed = 150;
int snowDuration = 5000;

bool foreground[WIDTH][HEIGHT];

// Win animation state
bool gameWon = false;
bool gameLost = false;
unsigned long winStartTime = 0;
int winPulseCount = 0;
int maxWinPulses = 5;
int baseBrightness = 20;
int maxBrightness = 100;
int winAnimationDuration = 8000;
int blizzardWinDuration = 4000;  // Shorter win animation for blizzard mode
int perfectDisplayDuration = 2000;  // Show "Perfect" for 2 seconds after disco

// Move tracking for perfect game detection
int moveCount = 0;
bool perfectGame = false;
bool showingPerfectDisplay = false;
unsigned long perfectDisplayStart = 0;

uint32_t discoColors[6];

#define MAX_SPARKLES 8
int sparkleX[MAX_SPARKLES];
int sparkleY[MAX_SPARKLES];
unsigned long sparkleStart[MAX_SPARKLES];
int sparkleDuration = 100;

// Lose animation state
bool losePhase1 = true;
unsigned long lastLoseFillUpdate = 0;
int loseFillSpeed = 50;
int frozenDuration = 5000;
unsigned long frozenStartTime = 0;

#define MAX_FALLING_LIGHTS 45
struct FallingLight {
  int x;
  float y;
  uint32_t color;
  bool active;
  float speed;
};
FallingLight fallingLights[MAX_FALLING_LIGHTS];
int numFallingLights = 0;
unsigned long lastLightFallUpdate = 0;
int lightFallSpeed = 40;

// Button configuration
#define BUTTON_0_PIN 10
#define BUTTON_1_PIN 11
#define BUTTON_2_PIN 12

int selectedSource = -1;
unsigned long lastButtonPress = 0;
unsigned long debounceDelay = 50;
bool lastButtonState[3] = {HIGH, HIGH, HIGH};
bool buttonState[3] = {HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[3] = {0, 0, 0};

// Tetris-style drop animation for moved rings
bool dropAnimActive = false;
unsigned long lastDropUpdate = 0;
int dropSpeed = 25;  // ms per row - fast but visible
int dropRingIndex = -1;
int dropTreeIndex = -1;
int dropTargetRow = -1;
int dropCurrentY = -1;  // Current Y position during drop (starts at -1, off screen)

// Landing flash effect
bool landingFlashActive = false;
unsigned long landingFlashStart = 0;
int landingFlashDuration = 80;  // ms for the white flash
int landingFlashRingIndex = -1;
int landingFlashTreeIndex = -1;
int landingFlashRow = -1;

int getFoliageRows() {
  return numRings;
}

int getFoliageStartRow() {
  return 5 - numRings;
}

int getTrunkRows() {
  if (numRings == 3) return 2;
  return 3;
}

int getTrunkStartRow() {
  return 5;
}

int getTrunkWidth() {
  if (numRings == 3) return 1;
  return 3;
}

int getRingWidthForPosition(int stackPos) {
  int ringIndex = (numRings - 1) - stackPos;
  return ringWidths[ringIndex];
}

int getRingIndexForPosition(int stackPos) {
  return (numRings - 1) - stackPos;
}

int xy(int x, int y) {
  int block = x / 4;
  int colInBlock = x % 4;
  int blockStart = block * 32;
  
  if (colInBlock % 2 == 0) {
    return blockStart + colInBlock * 8 + y;
  } else {
    return blockStart + colInBlock * 8 + (7 - y);
  }
}

void setPixel(int x, int y, uint32_t color) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
    strip.setPixelColor(xy(x, y), color);
  }
}

void setForegroundPixel(int x, int y, uint32_t color) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
    strip.setPixelColor(xy(x, y), color);
    foreground[x][y] = true;
  }
}

// 3x5 pixel font for digits 0-9
// Each digit is stored as 5 rows of 3 bits
const uint8_t digitFont[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111},  // 0
  {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
  {0b111, 0b001, 0b111, 0b100, 0b111},  // 2
  {0b111, 0b001, 0b111, 0b001, 0b111},  // 3
  {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
  {0b111, 0b100, 0b111, 0b001, 0b111},  // 5
  {0b111, 0b100, 0b111, 0b101, 0b111},  // 6
  {0b111, 0b001, 0b001, 0b001, 0b001},  // 7
  {0b111, 0b101, 0b111, 0b101, 0b111},  // 8
  {0b111, 0b101, 0b111, 0b001, 0b111},  // 9
};

// Draw a single digit at position (x, y) - y is top of digit
void drawDigit(int digit, int x, int y, uint32_t color) {
  if (digit < 0 || digit > 9) return;
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      if (digitFont[digit][row] & (0b100 >> col)) {
        setForegroundPixel(x + col, y + row, color);
      }
    }
  }
}

// Draw "Lvl" text at position - 3 chars, each 3 wide + 1 space = 11 pixels
void drawLvl(int x, int y, uint32_t color) {
  // L
  setForegroundPixel(x, y, color);
  setForegroundPixel(x, y+1, color);
  setForegroundPixel(x, y+2, color);
  setForegroundPixel(x, y+3, color);
  setForegroundPixel(x, y+4, color);
  setForegroundPixel(x+1, y+4, color);
  setForegroundPixel(x+2, y+4, color);
  
  // v (starts at x+4)
  setForegroundPixel(x+4, y+2, color);
  setForegroundPixel(x+4, y+3, color);
  setForegroundPixel(x+5, y+4, color);
  setForegroundPixel(x+6, y+3, color);
  setForegroundPixel(x+6, y+2, color);
  
  // l (starts at x+8)
  setForegroundPixel(x+8, y, color);
  setForegroundPixel(x+8, y+1, color);
  setForegroundPixel(x+8, y+2, color);
  setForegroundPixel(x+8, y+3, color);
  setForegroundPixel(x+8, y+4, color);
}

// Draw level number centered on screen
void drawLevelDisplay(int level, uint32_t textColor) {
  // Calculate width needed: "Lvl " (10px) + digits (4px each with spacing)
  int numDigits = 1;
  if (level >= 10) numDigits = 2;
  if (level >= 100) numDigits = 3;
  
  int totalWidth = 10 + numDigits * 4;  // Lvl + space + digits
  int startX = (WIDTH - totalWidth) / 2;
  int startY = 1;  // Vertically center in 8 rows (5 tall text + margins)
  
  drawLvl(startX, startY, textColor);
  
  // Draw number after "Lvl "
  int digitX = startX + 11;
  
  if (level >= 100) {
    drawDigit(level / 100, digitX, startY, textColor);
    digitX += 4;
  }
  if (level >= 10) {
    drawDigit((level / 10) % 10, digitX, startY, textColor);
    digitX += 4;
  }
  drawDigit(level % 10, digitX, startY, textColor);
}

// Draw "Perfect" centered on screen (7 letters, compact 3-wide each + spacing)
void drawPerfect(uint32_t textColor) {
  int startY = 1;
  int startX = 3;  // Start position for "Perfect" to fit in 32 pixels
  
  // P (x=3) - capital P
  setForegroundPixel(startX, startY, textColor);
  setForegroundPixel(startX, startY+1, textColor);
  setForegroundPixel(startX, startY+2, textColor);
  setForegroundPixel(startX, startY+3, textColor);
  setForegroundPixel(startX, startY+4, textColor);
  setForegroundPixel(startX+1, startY, textColor);
  setForegroundPixel(startX+2, startY, textColor);
  setForegroundPixel(startX+2, startY+1, textColor);
  setForegroundPixel(startX+1, startY+2, textColor);
  setForegroundPixel(startX+2, startY+2, textColor);
  
  // e (x=7) - lowercase e fitting in 5 rows (startY to startY+4)
  // Visual (open bottom right):
  //   0:  #     
  //   1: # #   
  //   2: ###    
  //   3: #      
  //   4:  #     
  startX = 7;
  setForegroundPixel(startX+1, startY, textColor);     // row 0: top
  setForegroundPixel(startX, startY+1, textColor);     // row 1: left
  setForegroundPixel(startX+2, startY+1, textColor);   // row 1: right
  setForegroundPixel(startX, startY+2, textColor);     // row 2: bar left
  setForegroundPixel(startX+1, startY+2, textColor);   // row 2: bar middle
  setForegroundPixel(startX+2, startY+2, textColor);   // row 2: bar right
  setForegroundPixel(startX, startY+3, textColor);     // row 3: left only
  setForegroundPixel(startX+1, startY+4, textColor);   // row 4: bottom (open right)
  
  // r (x=11) - lowercase r
  startX = 11;
  setForegroundPixel(startX, startY+1, textColor);
  setForegroundPixel(startX, startY+2, textColor);
  setForegroundPixel(startX, startY+3, textColor);
  setForegroundPixel(startX, startY+4, textColor);
  setForegroundPixel(startX+1, startY+1, textColor);
  setForegroundPixel(startX+2, startY+2, textColor);
  
  // f (x=15) - lowercase f
  startX = 15;
  setForegroundPixel(startX, startY+1, textColor);
  setForegroundPixel(startX, startY+2, textColor);
  setForegroundPixel(startX, startY+3, textColor);
  setForegroundPixel(startX, startY+4, textColor);
  setForegroundPixel(startX+1, startY, textColor);
  setForegroundPixel(startX+1, startY+2, textColor);
  setForegroundPixel(startX+2, startY, textColor);
  
  // e (x=19) - lowercase e fitting in 5 rows (startY to startY+4)
  // Visual (open bottom right):
  //   0:  #     
  //   1: # #   
  //   2: ###    
  //   3: #      
  //   4:  #     
  startX = 19;
  setForegroundPixel(startX+1, startY, textColor);     // row 0: top
  setForegroundPixel(startX, startY+1, textColor);     // row 1: left
  setForegroundPixel(startX+2, startY+1, textColor);   // row 1: right
  setForegroundPixel(startX, startY+2, textColor);     // row 2: bar left
  setForegroundPixel(startX+1, startY+2, textColor);   // row 2: bar middle
  setForegroundPixel(startX+2, startY+2, textColor);   // row 2: bar right
  setForegroundPixel(startX, startY+3, textColor);     // row 3: left only
  setForegroundPixel(startX+1, startY+4, textColor);   // row 4: bottom (open right)
  
  // c (x=23) - lowercase c
  startX = 23;
  setForegroundPixel(startX, startY+2, textColor);
  setForegroundPixel(startX, startY+3, textColor);
  setForegroundPixel(startX+1, startY+1, textColor);
  setForegroundPixel(startX+1, startY+4, textColor);
  setForegroundPixel(startX+2, startY+1, textColor);
  setForegroundPixel(startX+2, startY+4, textColor);
  
  // t (x=27) - lowercase t
  startX = 27;
  setForegroundPixel(startX, startY, textColor);
  setForegroundPixel(startX, startY+1, textColor);
  setForegroundPixel(startX, startY+2, textColor);
  setForegroundPixel(startX, startY+3, textColor);
  setForegroundPixel(startX+1, startY+1, textColor);
  setForegroundPixel(startX+1, startY+4, textColor);
  setForegroundPixel(startX+2, startY+4, textColor);
}

void generateRingPattern(int ringIndex) {
  int width = ringWidths[ringIndex];
  int lastColorIndex = -1;
  
  for (int i = 0; i < width; i++) {
    int colorIndex;
    bool isOuterPixel = (i == 0 || i == width - 1);
    do {
      colorIndex = random(4);
    } while (colorIndex == lastColorIndex || (isOuterPixel && colorIndex == 1));
    
    ringPatterns[ringIndex][i] = christmasColors[colorIndex];
    lastColorIndex = colorIndex;
  }
}

void initSnowExclusions() {
  for (int x = 0; x < WIDTH; x++) {
    snowExcluded[x] = false;
  }
  
  int trunkWidth = getTrunkWidth();
  for (int t = 0; t < 3; t++) {
    int centerX = treeCenters[t];
    if (trunkWidth == 1) {
      snowExcluded[centerX] = true;
    } else {
      snowExcluded[centerX - 1] = true;
      snowExcluded[centerX] = true;
      snowExcluded[centerX + 1] = true;
    }
  }
}

void drawTreeRow(int treeIndex, int screenRow, bool frozen) {
  int centerX = treeCenters[treeIndex];
  int foliageStartRow = getFoliageStartRow();
  int foliageRows = getFoliageRows();
  
  if (screenRow < foliageStartRow || screenRow >= foliageStartRow + foliageRows) {
    return;
  }
  
  int foliageIndex = screenRow - foliageStartRow;
  int rowWidth = ringWidths[foliageIndex];
  int startX = centerX - rowWidth / 2;
  
  uint32_t treeColor = frozen ? frozenGreen : green;
  
  for (int i = 0; i < rowWidth; i++) {
    setForegroundPixel(startX + i, screenRow, treeColor);
  }
  
  if (frozen) return;
  
  int stackPos = (foliageRows - 1) - foliageIndex;
  
  if (stackPos < pegHeights[treeIndex]) {
    int ringIndex = pegs[treeIndex][stackPos];
    int ringWidth = ringWidths[ringIndex];
    int ringStartX = centerX - ringWidth / 2;
    
    // Skip drawing if this ring is currently being drop-animated
    if (dropAnimActive && treeIndex == dropTreeIndex && ringIndex == dropRingIndex) {
      return;  // Don't draw ring at final position yet - it's still dropping
    }
    
    for (int i = 0; i < ringWidth; i++) {
      setForegroundPixel(ringStartX + i, screenRow, ringPatterns[ringIndex][i]);
    }
  }
}

void drawTree(int treeIndex, bool frozen) {
  int centerX = treeCenters[treeIndex];
  int trunkWidth = getTrunkWidth();
  int trunkRows = getTrunkRows();
  int trunkStartRow = getTrunkStartRow();
  int foliageStartRow = getFoliageStartRow();
  int foliageRows = getFoliageRows();
  
  uint32_t trunkColor = frozen ? frozenBrown : brown;
  
  for (int row = foliageStartRow; row < foliageStartRow + foliageRows; row++) {
    drawTreeRow(treeIndex, row, frozen);
  }
  
  int trunkStartX = centerX - trunkWidth / 2;
  for (int row = trunkStartRow; row < trunkStartRow + trunkRows; row++) {
    for (int i = 0; i < trunkWidth; i++) {
      setForegroundPixel(trunkStartX + i, row, trunkColor);
    }
  }
}

void clearForeground() {
  for (int x = 0; x < WIDTH; x++) {
    for (int y = 0; y < HEIGHT; y++) {
      foreground[x][y] = false;
    }
  }
}

void drawAllTrees() {
  strip.clear();
  clearForeground();
  for (int i = 0; i < 3; i++) {
    drawTree(i, false);
  }
}

void drawAllTreesFrozen() {
  strip.clear();
  clearForeground();
  for (int i = 0; i < 3; i++) {
    drawTree(i, true);
  }
}

void drawTreeDisco(int treeIndex, int colorOffset, bool flash) {
  int centerX = treeCenters[treeIndex];
  int trunkWidth = getTrunkWidth();
  int trunkRows = getTrunkRows();
  int trunkStartRow = getTrunkStartRow();
  int foliageStartRow = getFoliageStartRow();
  int foliageRows = getFoliageRows();
  
  for (int i = 0; i < foliageRows; i++) {
    int row = foliageStartRow + i;
    int rowWidth = ringWidths[i];
    int startX = centerX - rowWidth / 2;
    
    for (int j = 0; j < rowWidth; j++) {
      setForegroundPixel(startX + j, row, green);
    }
  }
  
  int trunkStartX = centerX - trunkWidth / 2;
  for (int row = trunkStartRow; row < trunkStartRow + trunkRows; row++) {
    for (int i = 0; i < trunkWidth; i++) {
      setForegroundPixel(trunkStartX + i, row, brown);
    }
  }
  
  for (int i = 0; i < foliageRows; i++) {
    int row = foliageStartRow + i;
    int rowWidth = ringWidths[i];
    int rowStartX = centerX - rowWidth / 2;
    
    for (int j = 0; j < rowWidth; j++) {
      if (flash && random(3) == 0) {
        setForegroundPixel(rowStartX + j, row, strip.Color(255, 255, 255));
      } else {
        int colorIndex = (j + i + colorOffset + treeIndex * 2) % 6;
        setForegroundPixel(rowStartX + j, row, discoColors[colorIndex]);
      }
    }
  }
}

void updateSparkles() {
  unsigned long now = millis();
  int foliageStartRow = getFoliageStartRow();
  int foliageRows = getFoliageRows();
  
  if (random(3) == 0) {
    for (int i = 0; i < MAX_SPARKLES; i++) {
      if (now - sparkleStart[i] > sparkleDuration) {
        int tree = random(3);
        int foliageIndex = random(foliageRows);
        int row = foliageStartRow + foliageIndex;
        int rowWidth = ringWidths[foliageIndex];
        int centerX = treeCenters[tree];
        int startX = centerX - rowWidth / 2;
        
        sparkleX[i] = startX + random(rowWidth);
        sparkleY[i] = row;
        sparkleStart[i] = now;
        break;
      }
    }
  }
}

void drawSparkles() {
  unsigned long now = millis();
  
  for (int i = 0; i < MAX_SPARKLES; i++) {
    if (now - sparkleStart[i] < sparkleDuration) {
      setPixel(sparkleX[i], sparkleY[i], strip.Color(255, 255, 255));
    }
  }
}

void drawAccumulatedSnow() {
  for (int x = 0; x < WIDTH; x++) {
    if (!gameLost && snowExcluded[x]) continue;
    
    for (int i = 0; i < snowAccumulation[x]; i++) {
      int y = HEIGHT - 1 - i;
      if (y >= 0 && !foreground[x][y]) {
        setPixel(x, y, snow);
      }
    }
  }
}

void initIntroSnow() {
  for (int i = 0; i < MAX_INTRO_SNOWFLAKES; i++) {
    introSnowActive[i] = false;
  }
  snowStartTime = millis();
}

void initTimerSnow() {
  for (int i = 0; i < MAX_TIMER_SNOWFLAKES; i++) {
    timerSnowActive[i] = false;
  }
}

void initSnowAccumulation() {
  for (int x = 0; x < WIDTH; x++) {
    snowAccumulation[x] = 0;
  }
}

void spawnIntroSnowflake() {
  for (int i = 0; i < MAX_INTRO_SNOWFLAKES; i++) {
    if (!introSnowActive[i]) {
      introSnowStartX[i] = random(WIDTH - 1);
      introSnowX[i] = introSnowStartX[i];
      introSnowY[i] = 0;
      introSnowActive[i] = true;
      return;
    }
  }
}

void spawnTimerSnowflake() {
  for (int i = 0; i < MAX_TIMER_SNOWFLAKES; i++) {
    if (!timerSnowActive[i]) {
      int attempts = 0;
      int startX;
      do {
        startX = random(WIDTH - 1);
        attempts++;
      } while (snowExcluded[startX] && attempts < 50);
      
      if (snowExcluded[startX]) return;
      
      timerSnowStartX[i] = startX;
      timerSnowX[i] = startX;
      timerSnowY[i] = 0;
      timerSnowActive[i] = true;
      return;
    }
  }
}

bool settleSnow(int x) {
  if (x < 0 || x >= WIDTH || snowExcluded[x]) return false;
  if (snowAccumulation[x] >= HEIGHT) return false;
  
  snowAccumulation[x]++;
  return true;
}

void updateIntroSnow() {
  for (int i = 0; i < MAX_INTRO_SNOWFLAKES; i++) {
    if (introSnowActive[i]) {
      introSnowY[i]++;
      
      if (introSnowY[i] % 2 == 0) {
        introSnowX[i] = introSnowStartX[i];
      } else {
        introSnowX[i] = introSnowStartX[i] + 1;
      }
      
      if (introSnowY[i] >= HEIGHT) {
        introSnowActive[i] = false;
      }
    }
  }
  
  unsigned long elapsed = millis() - snowStartTime;
  if (elapsed < snowDuration) {
    spawnIntroSnowflake();
    if (random(2) == 0) {
      spawnIntroSnowflake();
    }
  }
}

void updateTimerSnow() {
  for (int i = 0; i < MAX_TIMER_SNOWFLAKES; i++) {
    if (timerSnowActive[i]) {
      timerSnowY[i]++;
      
      if (timerSnowY[i] % 2 == 0) {
        timerSnowX[i] = timerSnowStartX[i];
      } else {
        timerSnowX[i] = timerSnowStartX[i] + 1;
      }
      
      int x = timerSnowX[i];
      
      if (x >= WIDTH) x = WIDTH - 1;
      if (x < 0) x = 0;
      
      int landingY = HEIGHT - 1 - snowAccumulation[x];
      
      if (timerSnowY[i] >= landingY) {
        timerSnowActive[i] = false;
        
        int settleX = -1;
        
        if (!snowExcluded[x] && snowAccumulation[x] < HEIGHT) {
          settleX = x;
        } else if (snowExcluded[x]) {
          for (int offset = 1; offset < WIDTH; offset++) {
            int leftX = x - offset;
            int rightX = x + offset;
            
            if (leftX >= 0 && !snowExcluded[leftX] && snowAccumulation[leftX] < HEIGHT) {
              settleX = leftX;
              break;
            }
            
            if (rightX < WIDTH && !snowExcluded[rightX] && snowAccumulation[rightX] < HEIGHT) {
              settleX = rightX;
              break;
            }
          }
        }
        
        if (settleX >= 0 && random(100) < snowSlideChance) {
          int leftX = settleX - 1;
          int rightX = settleX + 1;
          int currentHeight = snowAccumulation[settleX];
          
          bool canLeft = (leftX >= 0 && !snowExcluded[leftX] && snowAccumulation[leftX] < currentHeight);
          bool canRight = (rightX < WIDTH && !snowExcluded[rightX] && snowAccumulation[rightX] < currentHeight);
          
          if (canLeft && canRight) {
            if (snowAccumulation[leftX] < snowAccumulation[rightX]) {
              settleX = leftX;
            } else if (snowAccumulation[rightX] < snowAccumulation[leftX]) {
              settleX = rightX;
            } else {
              settleX = (random(2) == 0) ? leftX : rightX;
            }
          } else if (canLeft) {
            settleX = leftX;
          } else if (canRight) {
            settleX = rightX;
          }
        }
        
        if (settleX >= 0) {
          settleSnow(settleX);
        }
      }
    }
  }
}

void drawIntroSnow() {
  for (int i = 0; i < MAX_INTRO_SNOWFLAKES; i++) {
    if (introSnowActive[i]) {
      int x = introSnowX[i];
      int y = introSnowY[i];
      
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (!foreground[x][y]) {
          setPixel(x, y, snow);
        }
      }
    }
  }
}

void drawTimerSnow() {
  for (int i = 0; i < MAX_TIMER_SNOWFLAKES; i++) {
    if (timerSnowActive[i]) {
      int x = timerSnowX[i];
      int y = timerSnowY[i];
      
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (!foreground[x][y]) {
          setPixel(x, y, snow);
        }
      }
    }
  }
}

bool isIntroSnowFalling() {
  unsigned long elapsed = millis() - snowStartTime;
  if (elapsed < snowDuration) return true;
  
  for (int i = 0; i < MAX_INTRO_SNOWFLAKES; i++) {
    if (introSnowActive[i]) return true;
  }
  return false;
}

bool isSnowTimerExpired() {
  for (int x = 0; x < WIDTH; x++) {
    if (!snowExcluded[x] && snowAccumulation[x] >= HEIGHT) {
      return true;
    }
  }
  return false;
}

bool fillSnowOneStep() {
  bool allFull = true;
  for (int x = 0; x < WIDTH; x++) {
    if (snowAccumulation[x] < HEIGHT) {
      snowAccumulation[x]++;
      allFull = false;
    }
  }
  return allFull;
}

void initFallingLights() {
  numFallingLights = 0;
  int foliageStartRow = getFoliageStartRow();
  int foliageRows = getFoliageRows();
  
  for (int treeIndex = 0; treeIndex < 3; treeIndex++) {
    int centerX = treeCenters[treeIndex];
    
    for (int foliageIndex = 0; foliageIndex < foliageRows; foliageIndex++) {
      int screenRow = foliageStartRow + foliageIndex;
      int stackPos = (foliageRows - 1) - foliageIndex;
      
      if (stackPos < pegHeights[treeIndex]) {
        int ringIndex = pegs[treeIndex][stackPos];
        int ringWidth = ringWidths[ringIndex];
        int ringStartX = centerX - ringWidth / 2;
        
        for (int i = 0; i < ringWidth; i++) {
          if (numFallingLights < MAX_FALLING_LIGHTS) {
            fallingLights[numFallingLights].x = ringStartX + i;
            fallingLights[numFallingLights].y = (float)screenRow;
            fallingLights[numFallingLights].color = ringPatterns[ringIndex][i];
            fallingLights[numFallingLights].active = true;
            fallingLights[numFallingLights].speed = 0.15 + (random(10) / 100.0);
            numFallingLights++;
          }
        }
      }
    }
  }
}

void updateFallingLights() {
  for (int i = 0; i < numFallingLights; i++) {
    if (fallingLights[i].active) {
      fallingLights[i].y += fallingLights[i].speed;
      
      if (fallingLights[i].y >= HEIGHT) {
        fallingLights[i].active = false;
      }
    }
  }
}

void drawFallingLights() {
  for (int i = 0; i < numFallingLights; i++) {
    if (fallingLights[i].active) {
      int x = fallingLights[i].x;
      int y = (int)fallingLights[i].y;
      
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        setPixel(x, y, fallingLights[i].color);
      }
    }
  }
}

bool areLightsFalling() {
  for (int i = 0; i < numFallingLights; i++) {
    if (fallingLights[i].active) return true;
  }
  return false;
}

void startWinAnimation() {
  gameWon = true;
  winStartTime = millis();
  winPulseCount = 0;
  
  for (int i = 0; i < MAX_SPARKLES; i++) {
    sparkleStart[i] = 0;
  }
  
  // Check for perfect game (optimal number of moves)
  int optimalMoves = (1 << numRings) - 1;
  perfectGame = (moveCount == optimalMoves);
  
  Serial.println("*** YOU WIN! ***");
  Serial.print("Completed in ");
  Serial.print(moveCount);
  Serial.print(" moves (optimal: ");
  Serial.print(optimalMoves);
  Serial.println(")");
  
  if (perfectGame) {
    Serial.println("*** PERFECT! ***");
  }
  
  if (blizzardMode) {
    Serial.print("Blizzard Level ");
    Serial.print(blizzardLevel);
    Serial.println(" complete!");
    
    // Increase difficulty for next round
    blizzardLevel++;
    snowSpawnInterval = getBlizzardInterval(blizzardLevel);
    
    Serial.print("Next level snow rate: ");
    Serial.print(snowSpawnInterval);
    Serial.println("ms");
  }
}

void startLoseAnimation() {
  gameLost = true;
  winStartTime = millis();
  lastLoseFillUpdate = millis();
  losePhase1 = true;
  Serial.println("*** TIME'S UP - SNOWED IN! ***");
  
  if (blizzardMode) {
    Serial.print("Blizzard run ended at Level ");
    Serial.println(blizzardLevel);
    // Don't reset level - keep it for the level display
  }
}

void updateWinAnimation() {
  unsigned long elapsed = millis() - winStartTime;
  
  if (gameWon) {
    // Use shorter animation for blizzard mode
    int duration = blizzardMode ? blizzardWinDuration : winAnimationDuration;
    
    // Check if disco phase is done and we need to show Perfect
    if (perfectGame && !showingPerfectDisplay && elapsed >= duration) {
      // Transition to Perfect display
      showingPerfectDisplay = true;
      perfectDisplayStart = millis();
      return;
    }
    
    // If showing Perfect display
    if (showingPerfectDisplay) {
      if (millis() - perfectDisplayStart >= perfectDisplayDuration) {
        gameWon = false;
        showingPerfectDisplay = false;
        strip.setBrightness(baseBrightness);
        resetGame();
        return;
      }
      
      // Draw Perfect screen with snow background
      strip.clear();
      clearForeground();
      
      // Draw some gentle snow in background
      for (int i = 0; i < MAX_INTRO_SNOWFLAKES; i++) {
        if (introSnowActive[i]) {
          int x = introSnowX[i];
          int y = introSnowY[i];
          if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            setPixel(x, y, snow);
          }
        }
      }
      updateIntroSnow();
      
      drawPerfect(strip.Color(255, 255, 255));
      strip.show();
      return;
    }
    
    // Normal win - no perfect
    if (!perfectGame && elapsed >= duration) {
      gameWon = false;
      strip.setBrightness(baseBrightness);
      resetGame();
      return;
    }
    
    int colorOffset = (elapsed / 150) % 6;
    bool flash = (elapsed / 100) % 3 == 0;
    
    float pulseProgress = (elapsed % 500) / 500.0;
    float sineValue = sin(pulseProgress * PI * 2);
    int brightness = baseBrightness + (int)((maxBrightness - baseBrightness) * (0.5 + 0.5 * sineValue));
    strip.setBrightness(brightness);
    
    strip.clear();
    clearForeground();
    for (int i = 0; i < 3; i++) {
      drawTreeDisco(i, colorOffset, flash);
    }
    
    updateSparkles();
    drawSparkles();
    
    if (snowTimerMode) {
      drawAccumulatedSnow();
    }
    
    strip.show();
  }
  else if (gameLost) {
    if (losePhase1) {
      unsigned long now = millis();
      if (now - lastLoseFillUpdate >= loseFillSpeed) {
        lastLoseFillUpdate = now;
        
        bool allFull = fillSnowOneStep();
        
        drawAllTrees();
        drawAccumulatedSnow();
        strip.show();
        
        if (allFull) {
          losePhase1 = false;
          frozenStartTime = millis();
          lastLightFallUpdate = millis();
          
          initFallingLights();
          
          drawAllTreesFrozen();
          drawAccumulatedSnow();
          drawFallingLights();
          strip.show();
        }
      }
    } else {
      unsigned long now = millis();
      unsigned long elapsed = now - frozenStartTime;
      
      if (now - lastLightFallUpdate >= lightFallSpeed) {
        lastLightFallUpdate = now;
        updateFallingLights();
      }
      
      drawAllTreesFrozen();
      drawAccumulatedSnow();
      drawFallingLights();
      strip.show();
      
      if (elapsed >= frozenDuration && !areLightsFalling()) {
        gameLost = false;
        strip.setBrightness(baseBrightness);
        resetGame();
        return;
      }
    }
  }
}

// Blocking blink for source selection - simple and reliable
void blinkSourceRing(int treeIndex) {
  int stackPos = pegHeights[treeIndex] - 1;
  int ringIndex = pegs[treeIndex][stackPos];
  int ringWidth = ringWidths[ringIndex];
  int centerX = treeCenters[treeIndex];
  int ringStartX = centerX - ringWidth / 2;
  
  int foliageRows = getFoliageRows();
  int foliageStartRow = getFoliageStartRow();
  int foliageIndex = (foliageRows - 1) - stackPos;
  int screenRow = foliageStartRow + foliageIndex;
  
  // Hide ring (show green)
  for (int i = 0; i < ringWidth; i++) {
    setPixel(ringStartX + i, screenRow, green);
  }
  strip.show();
  delay(100);
  
  // Show ring again
  drawAllTrees();
  if (snowTimerMode) {
    drawAccumulatedSnow();
    drawTimerSnow();
  }
  strip.show();
}

void handleButtonPress(int treeIndex) {
  if (!gameStarted || gameWon || gameLost) {
    return;
  }
  
  if (selectedSource == -1) {
    if (pegHeights[treeIndex] == 0) {
      Serial.print("Tree ");
      Serial.print(treeIndex);
      Serial.println(" is empty!");
      return;
    }
    selectedSource = treeIndex;
    Serial.print("Selected source: Tree ");
    Serial.println(treeIndex);
    
    // Blink the source ring
    blinkSourceRing(treeIndex);
    
  } else {
    if (treeIndex == selectedSource) {
      Serial.println("Selection cancelled");
      selectedSource = -1;
      return;
    }
    
    Serial.print("Moving from Tree ");
    Serial.print(selectedSource);
    Serial.print(" to Tree ");
    Serial.println(treeIndex);
    
    moveRing(selectedSource, treeIndex);
    selectedSource = -1;
  }
}

int readButtons() {
  int buttonPins[3] = {BUTTON_0_PIN, BUTTON_1_PIN, BUTTON_2_PIN};
  
  for (int i = 0; i < 3; i++) {
    int reading = digitalRead(buttonPins[i]);
    
    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }
    
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        
        if (buttonState[i] == LOW) {
          lastButtonState[i] = reading;
          return i;
        }
      }
    }
    
    lastButtonState[i] = reading;
  }
  
  return -1;
}

bool checkWin() {
  return (pegHeights[1] == numRings || pegHeights[2] == numRings);
}

// Draw the dropping ring at its current animation position
void drawDroppingRing() {
  if (!dropAnimActive || dropCurrentY < 0) return;
  
  int centerX = treeCenters[dropTreeIndex];
  int ringWidth = ringWidths[dropRingIndex];
  int ringStartX = centerX - ringWidth / 2;
  
  // Draw the ring at its current Y position
  for (int i = 0; i < ringWidth; i++) {
    setForegroundPixel(ringStartX + i, dropCurrentY, ringPatterns[dropRingIndex][i]);
  }
}

// Update the drop animation - returns true if animation is still active
bool updateDropAnimation() {
  if (!dropAnimActive) return false;
  
  unsigned long now = millis();
  if (now - lastDropUpdate < dropSpeed) return true;
  lastDropUpdate = now;
  
  dropCurrentY++;
  
  // Check if we've reached the target row
  if (dropCurrentY >= dropTargetRow) {
    dropAnimActive = false;
    
    // Start landing flash
    landingFlashActive = true;
    landingFlashStart = millis();
    landingFlashRingIndex = dropRingIndex;
    landingFlashTreeIndex = dropTreeIndex;
    landingFlashRow = dropTargetRow;
    
    return false;
  }
  
  return true;
}

// Update landing flash - returns true if flash is active
bool updateLandingFlash() {
  if (!landingFlashActive) return false;
  
  if (millis() - landingFlashStart >= landingFlashDuration) {
    landingFlashActive = false;
    
    // Check for win after flash completes
    if (checkWin()) {
      startWinAnimation();
    }
    return false;
  }
  return true;
}

// Draw the ring with landing flash effect (white overlay that fades)
void drawLandingFlashRing() {
  if (!landingFlashActive) return;
  
  int centerX = treeCenters[landingFlashTreeIndex];
  int ringWidth = ringWidths[landingFlashRingIndex];
  int ringStartX = centerX - ringWidth / 2;
  
  // Calculate flash intensity (starts bright, fades out)
  unsigned long elapsed = millis() - landingFlashStart;
  float progress = (float)elapsed / landingFlashDuration;  // 0.0 to 1.0
  float flashIntensity = 1.0 - progress;  // 1.0 to 0.0
  
  for (int i = 0; i < ringWidth; i++) {
    // Get original color (NeoPixel stores as GRB, but Color() returns 0x00RRGGBB)
    uint32_t origColor = ringPatterns[landingFlashRingIndex][i];
    uint8_t r = (origColor >> 16) & 0xFF;
    uint8_t g = (origColor >> 8) & 0xFF;
    uint8_t b = origColor & 0xFF;
    
    // Blend toward white based on flash intensity
    uint8_t newR = r + (255 - r) * flashIntensity;
    uint8_t newG = g + (255 - g) * flashIntensity;
    uint8_t newB = b + (255 - b) * flashIntensity;
    
    setForegroundPixel(ringStartX + i, landingFlashRow, strip.Color(newR, newG, newB));
  }
}

// Start a drop animation for a ring
void startDropAnimation(int ringIndex, int treeIndex, int targetRow) {
  dropAnimActive = true;
  dropRingIndex = ringIndex;
  dropTreeIndex = treeIndex;
  dropTargetRow = targetRow;
  dropCurrentY = -1;  // Start off-screen (will become 0 on first update)
  lastDropUpdate = millis();
}

bool moveRing(int fromTree, int toTree) {
  if (!gameStarted) {
    Serial.println("Wait for game to start!");
    return false;
  }
  
  if (gameWon || gameLost) {
    Serial.println("Game over! Wait for reset...");
    return false;
  }
  
  if (fromTree < 0 || fromTree > 2 || toTree < 0 || toTree > 2) {
    Serial.println("Invalid tree (0-2)");
    return false;
  }
  
  if (fromTree == toTree) {
    Serial.println("Same tree!");
    return false;
  }
  
  if (pegHeights[fromTree] == 0) {
    Serial.println("No rings on source!");
    return false;
  }
  
  int ringIndex = pegs[fromTree][pegHeights[fromTree] - 1];
  
  if (pegHeights[toTree] > 0) {
    int topRingOnDest = pegs[toTree][pegHeights[toTree] - 1];
    if (ringIndex > topRingOnDest) {
      Serial.println("Can't place larger on smaller!");
      return false;
    }
  }
  
  // Start snow on first move in blizzard mode
  if (blizzardMode && blizzardWaitingForMove) {
    blizzardWaitingForMove = false;
    lastSnowSpawn = millis();
    Serial.println("Snow started!");
  }
  
  // Do the actual move
  pegs[fromTree][pegHeights[fromTree] - 1] = -1;
  pegHeights[fromTree]--;
  
  pegs[toTree][pegHeights[toTree]] = ringIndex;
  pegHeights[toTree]++;
  
  // Track move count
  moveCount++;
  
  // Calculate the target screen row for this ring
  // Stack position is pegHeights[toTree] - 1 (0-indexed from bottom)
  // Screen row is: foliageStartRow + (foliageRows - 1 - stackPos)
  int stackPos = pegHeights[toTree] - 1;
  int foliageStartRow = getFoliageStartRow();
  int foliageRows = getFoliageRows();
  int targetRow = foliageStartRow + (foliageRows - 1 - stackPos);
  
  // Start drop animation (win check happens when animation completes)
  startDropAnimation(ringIndex, toTree, targetRow);
  
  Serial.print("Move #");
  Serial.print(moveCount);
  Serial.print(": ring ");
  Serial.print(ringIndex);
  Serial.print(" tree ");
  Serial.print(fromTree);
  Serial.print(" -> ");
  Serial.println(toTree);
  
  // Immediate redraw to show the ring removed from source
  drawAllTrees();
  if (snowTimerMode) {
    drawAccumulatedSnow();
    drawTimerSnow();
  }
  strip.show();
  
  return true;
}

void printState() {
  Serial.println("=== State ===");
  Serial.print("Mode: n=");
  Serial.print(numRings);
  if (snowTimerMode) {
    Serial.println(" (snow timer mode)");
  } else {
    Serial.println(" (tutorial mode - no snow)");
  }
  
  for (int t = 0; t < 3; t++) {
    Serial.print("Tree ");
    Serial.print(t);
    Serial.print(" (bottom->top): ");
    for (int i = 0; i < pegHeights[t]; i++) {
      Serial.print(pegs[t][i]);
      if (i < pegHeights[t] - 1) Serial.print(", ");
    }
    if (pegHeights[t] == 0) Serial.print("empty");
    Serial.println();
  }
  
  if (snowTimerMode) {
    int maxSnow = 0;
    for (int x = 0; x < WIDTH; x++) {
      if (!snowExcluded[x] && snowAccumulation[x] > maxSnow) {
        maxSnow = snowAccumulation[x];
      }
    }
    Serial.print("Snow level: ");
    Serial.print(maxSnow);
    Serial.print("/");
    Serial.println(HEIGHT);
  }
  
  Serial.print("Optimal moves to solve: ");
  Serial.println((1 << numRings) - 1);
}

void resetGame() {
  for (int t = 0; t < 3; t++) {
    for (int i = 0; i < MAX_RINGS; i++) {
      pegs[t][i] = -1;
    }
    pegHeights[t] = 0;
  }
  
  for (int i = 0; i < numRings; i++) {
    pegs[0][i] = (numRings - 1) - i;
  }
  pegHeights[0] = numRings;
  
  initSnowExclusions();
  
  if (blizzardMode) {
    // Blizzard mode: show level display first, then wait for first move
    initIntroSnow();
    initTimerSnow();
    initSnowAccumulation();
    showingLevelDisplay = true;
    levelDisplayStart = millis();
    blizzardWaitingForMove = true;  // Snow won't start until first move
    gameStarted = false;
  } else if (snowTimerMode) {
    initIntroSnow();
    initTimerSnow();
    initSnowAccumulation();
    gameStarted = false;  // Wait for intro snow to finish
  } else {
    gameStarted = true;
  }
  
  gameWon = false;
  gameLost = false;
  dropAnimActive = false;
  landingFlashActive = false;
  selectedSource = -1;
  moveCount = 0;
  perfectGame = false;
  showingPerfectDisplay = false;
  lastSnowSpawn = millis();
  strip.setBrightness(baseBrightness);
  
  Serial.print("Game reset! n=");
  Serial.print(numRings);
  if (blizzardMode) {
    Serial.print(" - BLIZZARD Level ");
    Serial.print(blizzardLevel);
    Serial.print(" (");
    Serial.print(snowSpawnInterval);
    Serial.println("ms snow)");
  } else if (snowTimerMode) {
    Serial.println(" - Snow timer mode ON - don't get snowed in!");
  } else {
    Serial.print(" - Tutorial mode (");
    Serial.print((1 << numRings) - 1);
    Serial.println(" moves to solve)");
  }
  
  drawAllTrees();
  strip.show();
  
  if (gameStarted) {
    printState();
  }
}

void setNumRings(int n) {
  if (n < 3 || n > 5) {
    Serial.println("Invalid n! Use n=3, n=4, or n=5");
    return;
  }
  
  numRings = n;
  
  // Reset blizzard level when changing difficulty
  if (blizzardMode) {
    blizzardLevel = 1;
    snowSpawnInterval = getBlizzardInterval(blizzardLevel);
    Serial.println("Blizzard reset to Level 1");
  }
  
  for (int i = 1; i < MAX_RINGS; i++) {
    generateRingPattern(i);
  }
  
  resetGame();
}

void printHelp() {
  Serial.println("=== Holiday Towers of Hanoi ===");
  Serial.println();
  Serial.println("CONTROLS:");
  Serial.println("  Buttons: Press source tree, then destination");
  Serial.println("           Press same tree twice to cancel");
  Serial.println("  m XY    Move ring from tree X to Y (0/1/2)");
  Serial.println();
  Serial.println("GAME MODES:");
  Serial.println("  Tutorial - No snow, practice the puzzle");
  Serial.println("  Snow     - Race against accumulating snow");
  Serial.println("  Blizzard - Snow speeds up each level you beat");
  Serial.println();
  Serial.println("MODE COMMANDS:");
  Serial.println("  t       Toggle snow mode on/off");
  Serial.println("  b       Toggle blizzard mode on/off");
  Serial.println("  n=3/4/5 Set number of rings");
  Serial.println();
  Serial.println("SETTINGS:");
  Serial.println("  snow=N  Snow spawn rate (100-2000ms)");
  Serial.println("  drop=N  Drop animation speed (5-200ms)");
  Serial.println();
  Serial.println("OTHER: s=state, r=reset, w=test win, h=help");
  Serial.println();
  Serial.println("------ CURRENT STATUS ------");
  Serial.print("Rings: ");
  Serial.print(numRings);
  Serial.print(" (");
  Serial.print((1 << numRings) - 1);
  Serial.println(" moves optimal)");
  Serial.print("Mode: ");
  if (blizzardMode) {
    Serial.print("BLIZZARD Level ");
    Serial.print(blizzardLevel);
    Serial.print(" (");
    Serial.print(snowSpawnInterval);
    Serial.println("ms snow)");
  } else if (snowTimerMode) {
    Serial.print("Snow (");
    Serial.print(snowSpawnInterval);
    Serial.println("ms)");
  } else {
    Serial.println("Tutorial");
  }
}

void setup() {
  Serial.begin(115200);
  
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime) < 3000) {
    delay(10);
  }
  delay(500);
  
  strip.begin();
  strip.setBrightness(baseBrightness);
  strip.clear();
  
  randomSeed(analogRead(1));
  
  pinMode(BUTTON_0_PIN, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  
  christmasColors[0] = strip.Color(255, 0, 0);
  christmasColors[1] = strip.Color(0, 255, 0);
  christmasColors[2] = strip.Color(0, 0, 255);
  christmasColors[3] = strip.Color(255, 180, 0);
  
  discoColors[0] = strip.Color(255, 0, 0);
  discoColors[1] = strip.Color(255, 128, 0);
  discoColors[2] = strip.Color(255, 255, 0);
  discoColors[3] = strip.Color(0, 255, 0);
  discoColors[4] = strip.Color(0, 0, 255);
  discoColors[5] = strip.Color(255, 0, 255);
  
  green = strip.Color(0, 80, 0);
  brown = strip.Color(60, 30, 0);
  snow = strip.Color(60, 60, 70);
  frozenGreen = strip.Color(0, 0, 255);
  frozenBrown = strip.Color(0, 0, 200);
  
  ringPatterns[0][0] = strip.Color(255, 255, 255);
  
  for (int i = 1; i < MAX_RINGS; i++) {
    generateRingPattern(i);
  }
  
  resetGame();
  printHelp();
}

void loop() {
  // Track if drop animation was active (for redraw after completion)
  static bool wasDropping = false;
  
  if (gameWon || gameLost) {
    updateWinAnimation();
    delay(20);
  }
  else if (showingLevelDisplay && blizzardMode) {
    // Blizzard mode: show level with snow background
    unsigned long now = millis();
    
    if (now - lastSnowUpdate >= snowSpeed) {
      lastSnowUpdate = now;
      updateIntroSnow();
    }
    
    strip.clear();
    clearForeground();
    drawIntroSnow();
    drawLevelDisplay(blizzardLevel, strip.Color(255, 255, 255));
    strip.show();
    
    if (now - levelDisplayStart >= levelDisplayDuration) {
      showingLevelDisplay = false;
      gameStarted = true;
      lastSnowSpawn = millis();
      Serial.println("Ready to play! Snow starts on first move.");
      printState();
      drawAllTrees();
      strip.show();
      
      if (winGameImmediately) {
        startWinAnimation();
      }
    }
  }
  else if (!gameStarted && snowTimerMode) {
    unsigned long now = millis();
    if (now - lastSnowUpdate >= snowSpeed) {
      lastSnowUpdate = now;
      
      updateIntroSnow();
      drawAllTrees();
      drawIntroSnow();
      strip.show();
      
      if (!isIntroSnowFalling()) {
        gameStarted = true;
        lastSnowSpawn = millis();
        Serial.println("Ready to play!");
        printState();
        drawAllTrees();
        drawAccumulatedSnow();
        strip.show();
        
        if (winGameImmediately) {
          startWinAnimation();
        }
      }
    }
  }
  else if (gameStarted) {
    unsigned long now = millis();
    
    // Only accept button input when not animating
    if (!dropAnimActive && !landingFlashActive) {
      int pressedButton = readButtons();
      if (pressedButton >= 0) {
        handleButtonPress(pressedButton);
      }
    }
    
    if (snowTimerMode) {
      // In blizzard mode, only spawn snow after first move
      if (!blizzardWaitingForMove) {
        if (now - lastSnowSpawn >= snowSpawnInterval) {
          lastSnowSpawn = now;
          spawnTimerSnowflake();
        }
        
        if (now - lastSnowUpdate >= snowFallSpeed) {
          lastSnowUpdate = now;
          updateTimerSnow();
        }
      }
      
      // Always redraw in snow mode (snow is always moving)
      drawAllTrees();
      drawDroppingRing();  // Draw dropping ring on top
      drawLandingFlashRing();  // Draw flash effect on top
      drawAccumulatedSnow();
      drawTimerSnow();
      strip.show();
      
      if (!blizzardWaitingForMove && isSnowTimerExpired()) {
        startLoseAnimation();
      }
      
      // Update animations
      updateDropAnimation();
      updateLandingFlash();
    }
    else {
      // Non-snow modes: redraw while animating
      bool dropping = updateDropAnimation();
      bool flashing = updateLandingFlash();
      
      if (dropping || dropAnimActive || flashing || landingFlashActive) {
        drawAllTrees();
        drawDroppingRing();
        drawLandingFlashRing();
        strip.show();
        wasDropping = true;
      } else if (wasDropping) {
        // Final redraw after all animations complete
        drawAllTrees();
        strip.show();
        wasDropping = false;
      }
    }
  }
  
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    if (input.startsWith("n=") || input.startsWith("N=")) {
      if (input.length() >= 3) {
        char nChar = input.charAt(2);
        if (nChar >= '3' && nChar <= '5') {
          setNumRings(nChar - '0');
        } else {
          Serial.println("Use n=3, n=4, or n=5");
        }
      }
      return;
    }

    if (input.startsWith("snow=") || input.startsWith("SNOW=")) {
      int value = input.substring(5).toInt();
      if (value >= 100 && value <= 2000) {
        snowSpawnInterval = value;
        Serial.print("Snow spawn interval set to ");
        Serial.print(snowSpawnInterval);
        Serial.println("ms (lower = more snow)");
      } else {
        Serial.println("Use snow=100 to snow=2000 (ms between spawns)");
      }
      return;
    }

    if (input.startsWith("drop=") || input.startsWith("DROP=")) {
      int value = input.substring(5).toInt();
      if (value >= 5 && value <= 200) {
        dropSpeed = value;
        Serial.print("Drop speed set to ");
        Serial.print(dropSpeed);
        Serial.println("ms per row");
      } else {
        Serial.println("Use drop=5 to drop=200 (ms per row, lower = faster)");
      }
      return;
    }
    
    char cmd = input.charAt(0);
    
    if (cmd == 'm' || cmd == 'M') {
      int fromTree = -1;
      int toTree = -1;
      
      for (unsigned int i = 1; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c >= '0' && c <= '2') {
          if (fromTree == -1) {
            fromTree = c - '0';
          } else {
            toTree = c - '0';
            break;
          }
        }
      }
      
      if (fromTree >= 0 && toTree >= 0) {
        moveRing(fromTree, toTree);
      } else {
        Serial.println("Use: m01, m12, etc");
      }
    }
    else if (cmd == 's' || cmd == 'S') {
      printState();
    }
    else if (cmd == 'r' || cmd == 'R') {
      resetGame();
    }
    else if (cmd == 't' || cmd == 'T') {
      snowTimerMode = !snowTimerMode;
      Serial.print("Snow timer mode: ");
      Serial.println(snowTimerMode ? "ON" : "OFF");
      resetGame();
    }
    else if (cmd == 'b' || cmd == 'B') {
      blizzardMode = !blizzardMode;
      if (blizzardMode) {
        snowTimerMode = true;  // Blizzard requires snow
        blizzardLevel = 1;
        snowSpawnInterval = getBlizzardInterval(blizzardLevel);
        Serial.println("BLIZZARD MODE ON - snow gets faster each win!");
        Serial.print("Starting at Level 1, snow rate: ");
        Serial.print(snowSpawnInterval);
        Serial.println("ms");
      } else {
        Serial.println("Blizzard mode OFF");
        blizzardLevel = 1;  // Reset level when turning off
      }
      resetGame();
    }
    else if (cmd == 'w' || cmd == 'W') {
      Serial.println("Testing win animation...");
      startWinAnimation();
    }
    else if (cmd == 'h' || cmd == 'H') {
      printHelp();
    }
  }
}