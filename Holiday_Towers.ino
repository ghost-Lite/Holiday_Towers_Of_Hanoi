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
uint32_t frozenGreen;
uint32_t frozenBrown;

int treeCenters[3] = {4, 15, 26};

// Game state
int pegs[3][NUM_RINGS];
int pegHeights[3];

// ============ DEBUG CONFIGURATION ============
// Set to true to trigger win animation immediately after intro snow
bool winGameImmediately = false;

// ============ SNOW TIMER CONFIGURATION ============
// Set to true to enable snow accumulation timer mode
bool snowTimerMode = true;

// How often snow spawns (lower = more frequent = harder game)
// Recommended range: 300-800ms for playable difficulty
int snowSpawnInterval = 500;

// How fast snow falls (lower = faster)
int snowFallSpeed = 150;

// Chance (out of 100) that snow slides to a lower adjacent column when landing
// Higher = more leveling effect, 0 = pure random accumulation
int snowSlideChance = 40;

// ============ END SNOW TIMER CONFIG ============

// Intro snow state (aesthetic falling snow at game start)
#define MAX_INTRO_SNOWFLAKES 10
int introSnowX[MAX_INTRO_SNOWFLAKES];
int introSnowY[MAX_INTRO_SNOWFLAKES];
bool introSnowActive[MAX_INTRO_SNOWFLAKES];
int introSnowStartX[MAX_INTRO_SNOWFLAKES];

// Timer snow state (accumulating snow during gameplay)
#define MAX_TIMER_SNOWFLAKES 20
int timerSnowX[MAX_TIMER_SNOWFLAKES];
int timerSnowY[MAX_TIMER_SNOWFLAKES];
bool timerSnowActive[MAX_TIMER_SNOWFLAKES];
int timerSnowStartX[MAX_TIMER_SNOWFLAKES];

// Snow accumulation state (height of snow pile per column, 0 = no snow, 8 = full)
int snowAccumulation[WIDTH];

// Columns excluded from snow (tree trunk centers only)
bool snowExcluded[WIDTH];

bool gameStarted = false;
unsigned long lastSnowUpdate = 0;
unsigned long lastSnowSpawn = 0;
unsigned long snowStartTime = 0;
int snowSpeed = 150;  // Fall speed for intro animation
int snowDuration = 5000;  // Intro snow duration

// Foreground tracking
bool foreground[WIDTH][HEIGHT];

// Win animation state
bool gameWon = false;
bool gameLost = false;
unsigned long winStartTime = 0;
int winPulseCount = 0;
int maxWinPulses = 5;
int baseBrightness = 20;
int maxBrightness = 100;
int winAnimationDuration = 8000;  // Total win animation length (ms)

// Disco colors for win animation
uint32_t discoColors[6];

// Sparkle state for win animation
#define MAX_SPARKLES 8
int sparkleX[MAX_SPARKLES];
int sparkleY[MAX_SPARKLES];
unsigned long sparkleStart[MAX_SPARKLES];
int sparkleDuration = 100;  // How long each sparkle lasts

// Lose animation state
bool losePhase1 = true;  // true = filling snow, false = frozen trees
unsigned long lastLoseFillUpdate = 0;
int loseFillSpeed = 50;  // How fast snow fills (ms per row)
int frozenDuration = 5000;  // How long to show frozen trees (ms)
unsigned long frozenStartTime = 0;  // When phase 2 started

// Falling lights animation (during lose)
#define MAX_FALLING_LIGHTS 45  // 5 rows * 9 max width per tree = 45 possible lights
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
int lightFallSpeed = 40;  // ms between light fall updates

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

void initSnowExclusions() {
  // Mark all columns as available
  for (int x = 0; x < WIDTH; x++) {
    snowExcluded[x] = false;
  }
  
  // Exclude only center 3 pixels of each tree (the trunk area columns)
  for (int t = 0; t < 3; t++) {
    int centerX = treeCenters[t];
    snowExcluded[centerX - 1] = true;
    snowExcluded[centerX] = true;
    snowExcluded[centerX + 1] = true;
  }
}

void drawTreeRow(int treeIndex, int screenRow, bool frozen) {
  int centerX = treeCenters[treeIndex];
  int rowWidth = ringWidths[screenRow];
  int startX = centerX - rowWidth / 2;
  
  uint32_t treeColor = frozen ? frozenGreen : green;
  
  for (int i = 0; i < rowWidth; i++) {
    setForegroundPixel(startX + i, screenRow, treeColor);
  }
  
  // Don't draw ornaments/rings if frozen - they're falling off!
  if (frozen) return;
  
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

void drawTree(int treeIndex, bool frozen) {
  int centerX = treeCenters[treeIndex];
  
  uint32_t trunkColor = frozen ? frozenBrown : brown;
  
  for (int row = 0; row < 5; row++) {
    drawTreeRow(treeIndex, row, frozen);
  }
  
  int startX = centerX - 1;
  for (int i = 0; i < 3; i++) {
    setForegroundPixel(startX + i, 5, trunkColor);
    setForegroundPixel(startX + i, 6, trunkColor);
    setForegroundPixel(startX + i, 7, trunkColor);
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

// Draw tree with disco lights for win animation
void drawTreeDisco(int treeIndex, int colorOffset, bool flash) {
  int centerX = treeCenters[treeIndex];
  
  // Draw green foliage
  for (int row = 0; row < 5; row++) {
    int rowWidth = ringWidths[row];
    int startX = centerX - rowWidth / 2;
    
    for (int i = 0; i < rowWidth; i++) {
      setForegroundPixel(startX + i, row, green);
    }
  }
  
  // Draw trunk
  int startX = centerX - 1;
  for (int i = 0; i < 3; i++) {
    setForegroundPixel(startX + i, 5, brown);
    setForegroundPixel(startX + i, 6, brown);
    setForegroundPixel(startX + i, 7, brown);
  }
  
  // Draw disco lights on all 5 rows (full decorations!)
  for (int row = 0; row < 5; row++) {
    int rowWidth = ringWidths[row];
    int rowStartX = centerX - rowWidth / 2;
    
    for (int i = 0; i < rowWidth; i++) {
      if (flash && random(3) == 0) {
        // Random flash to white
        setForegroundPixel(rowStartX + i, row, strip.Color(255, 255, 255));
      } else {
        // Cycling disco colors
        int colorIndex = (i + row + colorOffset + treeIndex * 2) % 6;
        setForegroundPixel(rowStartX + i, row, discoColors[colorIndex]);
      }
    }
  }
}

// Add random sparkles
void updateSparkles() {
  unsigned long now = millis();
  
  // Spawn new sparkle
  if (random(3) == 0) {
    for (int i = 0; i < MAX_SPARKLES; i++) {
      if (now - sparkleStart[i] > sparkleDuration) {
        // Pick a random position on one of the trees
        int tree = random(3);
        int row = random(5);
        int rowWidth = ringWidths[row];
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

// Draw accumulated snow (behind foreground, visible above trees)
// During lose animation, draws on ALL columns
void drawAccumulatedSnow() {
  for (int x = 0; x < WIDTH; x++) {
    // During lose animation, draw snow everywhere (including excluded columns)
    // During normal play, skip excluded columns
    if (!gameLost && snowExcluded[x]) continue;
    
    // Draw from bottom up based on accumulation
    for (int i = 0; i < snowAccumulation[x]; i++) {
      int y = HEIGHT - 1 - i;  // Convert accumulation to screen row
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

// Spawn snowflake for timer mode (avoid excluded columns)
void spawnTimerSnowflake() {
  for (int i = 0; i < MAX_TIMER_SNOWFLAKES; i++) {
    if (!timerSnowActive[i]) {
      // Find a valid column (not excluded)
      int attempts = 0;
      int startX;
      do {
        startX = random(WIDTH - 1);
        attempts++;
      } while (snowExcluded[startX] && attempts < 50);
      
      if (snowExcluded[startX]) return;  // Couldn't find valid column
      
      timerSnowStartX[i] = startX;
      timerSnowX[i] = startX;
      timerSnowY[i] = 0;
      timerSnowActive[i] = true;
      return;
    }
  }
}

// Try to settle snow in a column, returns true if successful
bool settleSnow(int x) {
  if (x < 0 || x >= WIDTH || snowExcluded[x]) return false;
  if (snowAccumulation[x] >= HEIGHT) return false;
  
  snowAccumulation[x]++;
  return true;
}

// Update snow for intro animation (original behavior)
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

// Update snow for timer mode (accumulates behind and above trees)
void updateTimerSnow() {
  for (int i = 0; i < MAX_TIMER_SNOWFLAKES; i++) {
    if (timerSnowActive[i]) {
      timerSnowY[i]++;
      
      // Zigzag motion
      if (timerSnowY[i] % 2 == 0) {
        timerSnowX[i] = timerSnowStartX[i];
      } else {
        timerSnowX[i] = timerSnowStartX[i] + 1;
      }
      
      int x = timerSnowX[i];
      
      // Clamp x to valid range
      if (x >= WIDTH) x = WIDTH - 1;
      if (x < 0) x = 0;
      
      int landingY = HEIGHT - 1 - snowAccumulation[x];
      
      // Check if snowflake has reached the snow pile or bottom
      if (timerSnowY[i] >= landingY) {
        timerSnowActive[i] = false;
        
        int settleX = -1;
        
        // Settle in current column if not excluded and not full
        if (!snowExcluded[x] && snowAccumulation[x] < HEIGHT) {
          settleX = x;
        } else if (snowExcluded[x]) {
          // Only redirect if it's an excluded column (trunk)
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
        
        // Apply slide chance to level out accumulation
        if (settleX >= 0 && random(100) < snowSlideChance) {
          int leftX = settleX - 1;
          int rightX = settleX + 1;
          int currentHeight = snowAccumulation[settleX];
          
          // Slide TO lower neighbors (leveling effect)
          bool canLeft = (leftX >= 0 && !snowExcluded[leftX] && snowAccumulation[leftX] < currentHeight);
          bool canRight = (rightX < WIDTH && !snowExcluded[rightX] && snowAccumulation[rightX] < currentHeight);
          
          if (canLeft && canRight) {
            // Pick the LOWEST neighbor
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
        // Falling snow shows behind trees
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

// Check if snow has reached the top of any column
bool isSnowTimerExpired() {
  for (int x = 0; x < WIDTH; x++) {
    if (!snowExcluded[x] && snowAccumulation[x] >= HEIGHT) {
      return true;
    }
  }
  return false;
}

// Fill one row of snow across all columns (including excluded), returns true if all full
bool fillSnowOneStep() {
  bool allFull = true;
  for (int x = 0; x < WIDTH; x++) {
    // During lose animation, fill ALL columns including trunk columns
    if (snowAccumulation[x] < HEIGHT) {
      snowAccumulation[x]++;
      allFull = false;
    }
  }
  return allFull;
}

// Collect all Christmas lights from trees and prepare them for falling
void initFallingLights() {
  numFallingLights = 0;
  
  for (int treeIndex = 0; treeIndex < 3; treeIndex++) {
    int centerX = treeCenters[treeIndex];
    
    // Check each row of the tree (rows 0-4 have ornaments)
    for (int screenRow = 0; screenRow < 5; screenRow++) {
      int stackPos = 4 - screenRow;
      
      // Only collect lights if there's a ring at this position
      if (stackPos < pegHeights[treeIndex]) {
        int ringIndex = pegs[treeIndex][stackPos];
        int ringWidth = ringWidths[ringIndex];
        int ringStartX = centerX - ringWidth / 2;
        
        // Add each light from this ring
        for (int i = 0; i < ringWidth; i++) {
          if (numFallingLights < MAX_FALLING_LIGHTS) {
            fallingLights[numFallingLights].x = ringStartX + i;
            fallingLights[numFallingLights].y = (float)screenRow;
            fallingLights[numFallingLights].color = ringPatterns[ringIndex][i];
            fallingLights[numFallingLights].active = true;
            // Randomize speed slightly for more natural look
            fallingLights[numFallingLights].speed = 0.15 + (random(10) / 100.0);
            numFallingLights++;
          }
        }
      }
    }
  }
}

// Update falling lights positions
void updateFallingLights() {
  for (int i = 0; i < numFallingLights; i++) {
    if (fallingLights[i].active) {
      fallingLights[i].y += fallingLights[i].speed;
      
      // Check if light has fallen off screen
      if (fallingLights[i].y >= HEIGHT) {
        fallingLights[i].active = false;
      }
    }
  }
}

// Draw falling lights (they have priority over frozen trees)
void drawFallingLights() {
  for (int i = 0; i < numFallingLights; i++) {
    if (fallingLights[i].active) {
      int x = fallingLights[i].x;
      int y = (int)fallingLights[i].y;
      
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        // Lights draw on top of everything
        setPixel(x, y, fallingLights[i].color);
      }
    }
  }
}

// Check if any lights are still falling
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
  
  // Initialize sparkles
  for (int i = 0; i < MAX_SPARKLES; i++) {
    sparkleStart[i] = 0;
  }
  
  Serial.println("*** YOU WIN! ***");
}

void startLoseAnimation() {
  gameLost = true;
  winStartTime = millis();
  lastLoseFillUpdate = millis();
  losePhase1 = true;
  Serial.println("*** TIME'S UP - SNOWED IN! ***");
}

void updateWinAnimation() {
  unsigned long elapsed = millis() - winStartTime;
  
  // Win animation - disco party!
  if (gameWon) {
    if (elapsed >= winAnimationDuration) {
      gameWon = false;
      strip.setBrightness(baseBrightness);
      resetGame();
      return;
    }
    
    // Calculate color offset for cycling effect
    int colorOffset = (elapsed / 150) % 6;
    
    // Flash effect - more frequent flashing as time goes on
    bool flash = (elapsed / 100) % 3 == 0;
    
    // Pulse brightness
    float pulseProgress = (elapsed % 500) / 500.0;
    float sineValue = sin(pulseProgress * PI * 2);
    int brightness = baseBrightness + (int)((maxBrightness - baseBrightness) * (0.5 + 0.5 * sineValue));
    strip.setBrightness(brightness);
    
    // Draw disco trees
    strip.clear();
    clearForeground();
    for (int i = 0; i < 3; i++) {
      drawTreeDisco(i, colorOffset, flash);
    }
    
    // Update and draw sparkles
    updateSparkles();
    drawSparkles();
    
    // Draw accumulated snow if in timer mode
    if (snowTimerMode) {
      drawAccumulatedSnow();
    }
    
    strip.show();
  }
  // Lose animation - phase 1: fill snow, phase 2: frozen trees with falling lights
  else if (gameLost) {
    if (losePhase1) {
      // Phase 1: Rapidly fill snow to top
      unsigned long now = millis();
      if (now - lastLoseFillUpdate >= loseFillSpeed) {
        lastLoseFillUpdate = now;
        
        bool allFull = fillSnowOneStep();
        
        drawAllTrees();
        drawAccumulatedSnow();
        strip.show();
        
        if (allFull) {
          // Transition to phase 2
          losePhase1 = false;
          frozenStartTime = millis();
          lastLightFallUpdate = millis();
          
          // Collect all Christmas lights before they start falling
          initFallingLights();
          
          // Draw initial frozen scene
          drawAllTreesFrozen();
          drawAccumulatedSnow();
          drawFallingLights();  // Lights on top
          strip.show();
        }
      }
    } else {
      // Phase 2: Show frozen trees with falling lights
      unsigned long now = millis();
      unsigned long elapsed = now - frozenStartTime;
      
      // Update falling lights
      if (now - lastLightFallUpdate >= lightFallSpeed) {
        lastLightFallUpdate = now;
        updateFallingLights();
      }
      
      // Keep showing frozen scene with falling lights
      drawAllTreesFrozen();
      drawAccumulatedSnow();
      drawFallingLights();  // Lights have priority, draw on top
      strip.show();
      
      // Only reset after frozen duration AND all lights have fallen
      if (elapsed >= frozenDuration && !areLightsFalling()) {
        // Animation done, reset game
        gameLost = false;
        strip.setBrightness(baseBrightness);
        resetGame();
        return;
      }
    }
  }
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
  if (snowTimerMode) {
    drawAccumulatedSnow();
  }
  drawTimerSnow();
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
  
  if (snowTimerMode) {
    // Find max accumulation for status
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
  
  initIntroSnow();
  initTimerSnow();
  initSnowAccumulation();
  gameStarted = false;
  gameWon = false;
  gameLost = false;
  lastSnowSpawn = millis();
  strip.setBrightness(baseBrightness);
  
  if (snowTimerMode) {
    Serial.println("Game reset! Snow timer mode ON - don't get snowed in!");
  } else {
    Serial.println("Game reset! Snow falling...");
  }
  drawAllTrees();
  strip.show();
}

void printHelp() {
  Serial.println("=== Holiday Towers of Hanoi ===");
  Serial.println("m XY - Move top ring from tree X to Y");
  Serial.println("s    - Show state");
  Serial.println("r    - Reset game");
  Serial.println("t    - Toggle snow timer mode");
  Serial.println("w    - Test win animation");
  Serial.println("h    - Help");
  Serial.println("Trees: 0=left, 1=middle, 2=right");
  Serial.println("Goal: Stack all rings on tree 1 or 2");
  if (snowTimerMode) {
    Serial.println("Timer mode: ON - Beat the snow!");
  } else {
    Serial.println("Timer mode: OFF - Relaxed play");
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
  
  christmasColors[0] = strip.Color(255, 0, 0);
  christmasColors[1] = strip.Color(0, 255, 0);
  christmasColors[2] = strip.Color(0, 0, 255);
  christmasColors[3] = strip.Color(255, 180, 0);
  
  // Disco colors for win animation
  discoColors[0] = strip.Color(255, 0, 0);     // Red
  discoColors[1] = strip.Color(255, 128, 0);   // Orange
  discoColors[2] = strip.Color(255, 255, 0);   // Yellow
  discoColors[3] = strip.Color(0, 255, 0);     // Green
  discoColors[4] = strip.Color(0, 0, 255);     // Blue
  discoColors[5] = strip.Color(255, 0, 255);   // Magenta
  
  green = strip.Color(0, 80, 0);
  brown = strip.Color(60, 30, 0);
  snow = strip.Color(60, 60, 70);
  frozenGreen = strip.Color(0, 0, 255);  // Blue frozen foliage
  frozenBrown = strip.Color(0, 0, 200);  // Dimmer blue frozen trunk
  
  ringPatterns[0][0] = strip.Color(255, 255, 255);
  
  for (int i = 1; i < NUM_RINGS; i++) {
    generateRingPattern(i);
  }
  
  initSnowExclusions();
  resetGame();
  printHelp();
}

void loop() {
  // Win/Lose animation takes priority
  if (gameWon || gameLost) {
    updateWinAnimation();
    delay(20);  // Smooth animation
  }
  // Intro snow animation (before game starts)
  else if (!gameStarted) {
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
        if (snowTimerMode) {
          drawAccumulatedSnow();
        }
        strip.show();
        
        // Debug: trigger win immediately if flag is set
        if (winGameImmediately) {
          startWinAnimation();
        }
      }
    }
  }
  // Game in progress
  else {
    unsigned long now = millis();
    
    // Snow timer mode: spawn and update falling snow
    if (snowTimerMode) {
      // Spawn new snowflakes at intervals
      if (now - lastSnowSpawn >= snowSpawnInterval) {
        lastSnowSpawn = now;
        spawnTimerSnowflake();
      }
      
      // Update falling snow
      if (now - lastSnowUpdate >= snowFallSpeed) {
        lastSnowUpdate = now;
        updateTimerSnow();
        
        // Redraw everything
        drawAllTrees();
        drawAccumulatedSnow();
        drawTimerSnow();
        strip.show();
        
        // Check for game over
        if (isSnowTimerExpired()) {
          startLoseAnimation();
        }
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
    else if (cmd == 't' || cmd == 'T') {
      snowTimerMode = !snowTimerMode;
      Serial.print("Snow timer mode: ");
      Serial.println(snowTimerMode ? "ON" : "OFF");
      resetGame();
    }
    else if (cmd == 'w' || cmd == 'W') {
      // Debug: manually trigger win animation
      Serial.println("Testing win animation...");
      startWinAnimation();
    }
    else if (cmd == 'h' || cmd == 'H') {
      printHelp();
    }
  }
}