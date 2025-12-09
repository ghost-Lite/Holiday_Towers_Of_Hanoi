#include <Adafruit_NeoPixel.h>

#define PIN 2
#define NUM_LEDS 256
#define WIDTH 32
#define HEIGHT 8

Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

#define NUM_RINGS 5
int ringWidths[NUM_RINGS] = {1, 3, 5, 7, 9};

uint32_t ringPatterns[NUM_RINGS][9];

uint32_t christmasColors[4];
uint32_t green;
uint32_t brown;
uint32_t snow;

int treeCenters[3] = {4, 15, 26};

// Game state
int pegs[3][NUM_RINGS];
int pegHeights[3];

// Snow state
#define MAX_SNOWFLAKES 10
int snowX[MAX_SNOWFLAKES];
int snowY[MAX_SNOWFLAKES];
bool snowActive[MAX_SNOWFLAKES];
int snowStartX[MAX_SNOWFLAKES];

bool gameStarted = false;
unsigned long lastSnowUpdate = 0;
unsigned long snowStartTime = 0;
int snowSpeed = 150;
int snowDuration = 5000;

// Foreground tracking
bool foreground[WIDTH][HEIGHT];

// Win animation state
bool gameWon = false;
unsigned long winStartTime = 0;
int winPulseCount = 0;
int maxWinPulses = 5;
int baseBrightness = 20;
int maxBrightness = 100;

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

void generateRingPattern(int ringIndex) {
  int width = ringWidths[ringIndex];
  int lastColorIndex = -1;
  
  for (int i = 0; i < width; i++) {
    int colorIndex;
    do {
      colorIndex = random(4);
    } while (colorIndex == lastColorIndex);
    
    ringPatterns[ringIndex][i] = christmasColors[colorIndex];
    lastColorIndex = colorIndex;
  }
}

void drawTreeRow(int treeIndex, int screenRow) {
  int centerX = treeCenters[treeIndex];
  int rowWidth = ringWidths[screenRow];
  int startX = centerX - rowWidth / 2;
  
  for (int i = 0; i < rowWidth; i++) {
    setForegroundPixel(startX + i, screenRow, green);
  }
  
  int stackPos = 4 - screenRow;
  
  if (stackPos < pegHeights[treeIndex]) {
    int ringIndex = pegs[treeIndex][stackPos];
    int ringWidth = ringWidths[ringIndex];
    int ringStartX = centerX - ringWidth / 2;
    
    for (int i = 0; i < ringWidth; i++) {
      setForegroundPixel(ringStartX + i, screenRow, ringPatterns[ringIndex][i]);
    }
  }
}

void drawTree(int treeIndex) {
  int centerX = treeCenters[treeIndex];
  
  for (int row = 0; row < 5; row++) {
    drawTreeRow(treeIndex, row);
  }
  
  int startX = centerX - 1;
  for (int i = 0; i < 3; i++) {
    setForegroundPixel(startX + i, 5, brown);
    setForegroundPixel(startX + i, 6, brown);
    setForegroundPixel(startX + i, 7, brown);
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
    drawTree(i);
  }
}

void initSnow() {
  for (int i = 0; i < MAX_SNOWFLAKES; i++) {
    snowActive[i] = false;
  }
  snowStartTime = millis();
}

void spawnSnowflake() {
  for (int i = 0; i < MAX_SNOWFLAKES; i++) {
    if (!snowActive[i]) {
      snowStartX[i] = random(WIDTH - 1);
      snowX[i] = snowStartX[i];
      snowY[i] = 0;
      snowActive[i] = true;
      return;
    }
  }
}

void updateSnow() {
  for (int i = 0; i < MAX_SNOWFLAKES; i++) {
    if (snowActive[i]) {
      snowY[i]++;
      
      if (snowY[i] % 2 == 0) {
        snowX[i] = snowStartX[i];
      } else {
        snowX[i] = snowStartX[i] + 1;
      }
      
      if (snowY[i] >= HEIGHT) {
        snowActive[i] = false;
      }
    }
  }
  
  unsigned long elapsed = millis() - snowStartTime;
  if (elapsed < snowDuration) {
    spawnSnowflake();
    if (random(2) == 0) {
      spawnSnowflake();
    }
  }
}

void drawSnow() {
  for (int i = 0; i < MAX_SNOWFLAKES; i++) {
    if (snowActive[i]) {
      int x = snowX[i];
      int y = snowY[i];
      
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (!foreground[x][y]) {
          setPixel(x, y, snow);
        }
      }
    }
  }
}

bool isSnowFalling() {
  unsigned long elapsed = millis() - snowStartTime;
  if (elapsed < snowDuration) return true;
  
  for (int i = 0; i < MAX_SNOWFLAKES; i++) {
    if (snowActive[i]) return true;
  }
  return false;
}

void startWinAnimation() {
  gameWon = true;
  winStartTime = millis();
  winPulseCount = 0;
  Serial.println("*** YOU WIN! ***");
}

void updateWinAnimation() {
  unsigned long elapsed = millis() - winStartTime;
  int pulseDuration = 1000;  // 1 second per pulse
  
  int currentPulse = elapsed / pulseDuration;
  if (currentPulse >= maxWinPulses) {
    // Animation done, reset game
    gameWon = false;
    strip.setBrightness(baseBrightness);
    resetGame();
    return;
  }
  
  // Calculate brightness using sine wave for smooth pulse
  float pulseProgress = (elapsed % pulseDuration) / (float)pulseDuration;
  float sineValue = sin(pulseProgress * PI);  // 0 -> 1 -> 0
  int brightness = baseBrightness + (int)((maxBrightness - baseBrightness) * sineValue);
  
  strip.setBrightness(brightness);
  drawAllTrees();
  strip.show();
}

bool checkWin() {
  // Win if tree 1 or tree 2 has all 5 rings
  return (pegHeights[1] == NUM_RINGS || pegHeights[2] == NUM_RINGS);
}

bool moveRing(int fromTree, int toTree) {
  if (!gameStarted) {
    Serial.println("Wait for snow to finish!");
    return false;
  }
  
  if (gameWon) {
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
  
  pegs[fromTree][pegHeights[fromTree] - 1] = -1;
  pegHeights[fromTree]--;
  
  pegs[toTree][pegHeights[toTree]] = ringIndex;
  pegHeights[toTree]++;
  
  Serial.print("Moved ring ");
  Serial.print(ringIndex);
  Serial.print(": tree ");
  Serial.print(fromTree);
  Serial.print(" -> ");
  Serial.println(toTree);
  
  drawAllTrees();
  strip.show();
  
  if (checkWin()) {
    startWinAnimation();
  }
  
  return true;
}

void printState() {
  Serial.println("=== State ===");
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
}

void resetGame() {
  for (int t = 0; t < 3; t++) {
    for (int i = 0; i < NUM_RINGS; i++) {
      pegs[t][i] = -1;
    }
    pegHeights[t] = 0;
  }
  
  for (int i = 0; i < NUM_RINGS; i++) {
    pegs[0][i] = 4 - i;
  }
  pegHeights[0] = NUM_RINGS;
  
  initSnow();
  gameStarted = false;
  gameWon = false;
  strip.setBrightness(baseBrightness);
  
  Serial.println("Game reset! Snow falling...");
  drawAllTrees();
  strip.show();
}

void printHelp() {
  Serial.println("=== Holiday Towers of Hanoi ===");
  Serial.println("m XY - Move top ring from tree X to Y");
  Serial.println("s    - Show state");
  Serial.println("r    - Reset game");
  Serial.println("h    - Help");
  Serial.println("Trees: 0=left, 1=middle, 2=right");
  Serial.println("Goal: Stack all rings on tree 1 or 2");
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
  
  christmasColors[0] = strip.Color(255, 0, 0);
  christmasColors[1] = strip.Color(0, 255, 0);
  christmasColors[2] = strip.Color(0, 0, 255);
  christmasColors[3] = strip.Color(255, 180, 0);
  
  green = strip.Color(0, 80, 0);
  brown = strip.Color(60, 30, 0);
  snow = strip.Color(60, 60, 70);
  
  ringPatterns[0][0] = strip.Color(255, 255, 255);
  
  for (int i = 1; i < NUM_RINGS; i++) {
    generateRingPattern(i);
  }
  
  resetGame();
  printHelp();
}

void loop() {
  // Win animation takes priority
  if (gameWon) {
    updateWinAnimation();
    delay(20);  // Smooth animation
  }
  // Snow animation
  else if (!gameStarted) {
    unsigned long now = millis();
    if (now - lastSnowUpdate >= snowSpeed) {
      lastSnowUpdate = now;
      
      updateSnow();
      drawAllTrees();
      drawSnow();
      strip.show();
      
      if (!isSnowFalling()) {
        gameStarted = true;
        Serial.println("Ready to play!");
        printState();
        drawAllTrees();
        strip.show();
      }
    }
  }
  
  // Handle serial input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
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
    else if (cmd == 'h' || cmd == 'H') {
      printHelp();
    }
  }
}