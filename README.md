# Holiday Towers of Hanoi

A Christmas-themed Towers of Hanoi puzzle game displayed on an 8x32 NeoPixel LED matrix, controlled by an ESP32-S3 SuperMini. Three Christmas trees serve as the pegs, with colorful ornament "rings" that players move between trees.

![License](https://img.shields.io/badge/license-MIT-blue.svg)

## Features

- **Three difficulty levels**: n=3 (7 moves), n=4 (15 moves), n=5 (31 moves)
- **Snow timer mode** (n=5 only): Falling snow accumulates as a visual timer—if any column fills to the top, you lose!
- **Win animation**: Disco party with color cycling, brightness pulsing, and sparkles
- **Lose animation**: Screen fills with snow, trees freeze blue, ornaments fall off
- **Physical button controls**: Three buttons (one per tree) for intuitive gameplay
- **Serial interface**: Full control via terminal commands for testing and educational use
- **Educational mode**: External microcontroller can send moves to demonstrate recursive algorithms

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| LED Matrix | 8x32 NeoPixel/WS2812 (256 LEDs) |
| Microcontroller | ESP32-S3 SuperMini |
| Power Supply | 5V 10A+ (with direct power injection) |
| Buttons | 3× momentary pushbuttons |

### Wiring

**LED Matrix:**
- DIN → GPIO 2
- 5V → External power supply
- GND → Common ground with ESP32

**Buttons (directly to ESP32):**
- Button 0 (Left tree) → GPIO 10
- Button 1 (Middle tree) → GPIO 11
- Button 2 (Right tree) → GPIO 12
- All buttons connect to GND when pressed (uses internal pull-ups)

### Matrix Wiring Configuration

The matrix is wired in blocks of 4 columns (32 LEDs per block) with a zigzag pattern:

```
Block 0 (cols 0-3)   Block 1 (cols 4-7)   ...
[0]  [15] [16] [31]  [32] [47] [48] [63]
[1]  [14] [17] [30]  [33] [46] [49] [62]
[2]  [13] [18] [29]  [34] [45] [50] [61]
[3]  [12] [19] [28]  [35] [44] [51] [60]
[4]  [11] [20] [27]  [36] [43] [52] [59]
[5]  [10] [21] [26]  [37] [42] [53] [58]
[6]  [9]  [22] [25]  [38] [41] [54] [57]
[7]  [8]  [23] [24]  [39] [40] [55] [56]
```

## Installation

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/)
2. Install the **Adafruit NeoPixel** library
3. Select board: **ESP32-S3 Dev Module** (or your specific ESP32-S3 variant)
4. Upload `Holiday_Towers_Of_Hanoi.ino` to your ESP32-S3

## How to Play

### Button Controls
1. Press a tree button to select it as the source (the top ring will blink)
2. Press another tree button to move the ring to that destination
3. Press the same button twice to cancel your selection

### Rules
- You can only move the top ring from any tree
- You cannot place a larger ring on top of a smaller ring
- Goal: Move all rings from the left tree to the middle or right tree

### Difficulty Modes
- **n=3**: 3 rings, beginner mode (minimum 7 moves)
- **n=4**: 4 rings, intermediate mode (minimum 15 moves)
- **n=5**: 5 rings with snow timer, full challenge (minimum 31 moves)

## Serial Commands

Connect at **115200 baud** to use these commands:

| Command | Description |
|---------|-------------|
| `m01` | Move top ring from tree 0 to tree 1 |
| `m12` | Move top ring from tree 1 to tree 2 |
| `m20` | Move top ring from tree 2 to tree 0 |
| `n=3` | Set difficulty to 3 rings |
| `n=4` | Set difficulty to 4 rings |
| `n=5` | Set difficulty to 5 rings (with snow) |
| `snow=X` | Set snow spawn interval (milliseconds) |
| `t` | Toggle snow timer on/off |
| `r` | Reset game |
| `s` | Print current game state |
| `w` | Trigger win animation (for testing) |
| `h` | Show help |

## Educational Use

This project is designed to teach recursive algorithms through hands-on interaction.

### External Controller Interface

Students can connect an Arduino or another ESP32 to control the game programmatically. Wire the student's microcontroller to the same GPIO pins as the buttons (GPIO 10, 11, 12) and have it send LOW signals to simulate button presses.

**Student controller setup:**
```cpp
// Student's Arduino/ESP32
void setup() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);  // Idle state
}

void pressButton(int pin) {
  digitalWrite(pin, LOW);
  delay(100);
  digitalWrite(pin, HIGH);
  delay(200);  // Debounce delay
}
```

This allows students to implement and visualize the recursive Tower of Hanoi algorithm:

```cpp
void hanoi(int n, int source, int dest, int aux) {
  if (n == 1) {
    pressButton(source);  // Select source
    delay(500);
    pressButton(dest);    // Move to destination
    delay(500);
    return;
  }
  hanoi(n - 1, source, aux, dest);
  pressButton(source);
  delay(500);
  pressButton(dest);
  delay(500);
  hanoi(n - 1, aux, dest, source);
}
```

## Visual Design

- **Trees**: Green foliage with brown trunks, sized based on difficulty
- **Rings**: Randomized Christmas colors (red, blue, gold, orange) with white top
- **Snow**: White flakes that fall and accumulate (n=5 mode)
- **Tree positions**: Centered at x = 4, 15, 26 on the 32-pixel wide display

## Dependencies

- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

Jacob Marks - [GhostLite LLC](https://github.com/ghostlite)

## Acknowledgments

- Inspired by the classic Towers of Hanoi puzzle
- Built for holiday decoration and STEM education
