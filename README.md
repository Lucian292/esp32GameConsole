# ESP32 Game Console

A small ESP32 game console framework built with PlatformIO and tested in Wokwi. The project uses a 128x64 SSD1306 OLED, eight push buttons, a buzzer, and a servo to create a simple handheld-style menu system with two playable games: Snake and Tetris.

The code is structured as a compact console shell in `src/main.cpp` plus separate game modules for each title. The main menu also includes a persistent servo lock/unlock flow and utility screens for input testing and settings.

## Features

- Main menu with Snake, Tetris, Settings, and Lock entries
- Modular game screens for Snake and Tetris
- 128x64 SSD1306 OLED interface over I2C
- Debounced active-low button input
- Audio feedback through a buzzer
- Servo control with persistent lock/unlock state
- Input test screen for verifying all buttons
- Settings screen with servo pulse test and buzzer test
- Wokwi diagram arranged manually for a clean visual layout

## Hardware

The current build targets an ESP32 DevKit V1.

| Component | Pin / Connection | Notes |
| --- | --- | --- |
| OLED SSD1306 | SDA = GPIO 21 | I2C data line |
| OLED SSD1306 | SCL = GPIO 22 | I2C clock line |
| OLED SSD1306 | VIN = 3V3 | Powered from 3.3V |
| OLED SSD1306 | GND = GND | Common ground |
| OLED SSD1306 | Address = 0x3C | Configured in code and Wokwi |
| Buzzer | GPIO 15 | Sound feedback |
| Servo PWM | GPIO 13 | Unlock/lock actuator |
| Servo V+ | VIN | Powered from board supply |
| Servo GND | GND | Common ground |
| Button Up | GPIO 32 | Active-low with internal pull-up |
| Button Down | GPIO 33 | Active-low with internal pull-up |
| Button Left | GPIO 25 | Active-low with internal pull-up |
| Button Right | GPIO 26 | Active-low with internal pull-up |
| Button A | GPIO 27 | Active-low with internal pull-up |
| Button B | GPIO 14 | Active-low with internal pull-up |
| Button Start | GPIO 18 | Active-low with internal pull-up |
| Button Select | GPIO 19 | Active-low with internal pull-up |

## Controls

### Main Menu

- Up / Down: move through menu items
- A or Start: open the selected item
- Lock: force the servo back to the locked position

### Snake

- Up / Down / Left / Right: move the snake
- B or Select: return to the main menu
- A or Start on game over: restart the game
- B or Select on game over: return to the menu

Snake unlocks the servo once the score reaches 3.

### Tetris

- Left / Right: move the current piece
- Up or A: rotate the piece
- Down: soft drop
- Start: hard drop
- B or Select: return to the main menu
- A or Start on game over: restart the game
- B or Select on game over: return to the menu

Tetris awards 100 points per cleared line and unlocks the servo once the score reaches 100.

### Settings

- Up / Down: move through settings items
- A or Start: activate the selected setting
- B or Select: return to the main menu

Settings currently includes:

- Servo test: pulses the servo briefly and then restores the previous lock state
- Buzzer test: plays a short tone
- Back: returns to the main menu

## Gameplay Notes

### Snake

Snake runs on a compact grid that fits the OLED screen. Food increases the score and triggers a food sound. When the score threshold is reached, the servo is unlocked and stays unlocked until the user selects Lock from the main menu.

Current Snake unlock threshold: 3 points.

### Tetris

Tetris uses a small board sized for the OLED display. Each cleared line increases the score, plays a feedback tone, and contributes toward the unlock threshold. The game ends when a new piece cannot spawn because the stack reaches the top.

Current Tetris unlock threshold: 100 points.

## Project Structure

```text
.
├── diagram.json
├── platformio.ini
├── wokwi.toml
├── README.md
├── src
│   ├── main.cpp
│   ├── SnakeGame.h
│   ├── SnakeGame.cpp
│   ├── TetrisGame.h
│   └── TetrisGame.cpp
├── include
├── lib
└── test
```

## Build and Run

### PlatformIO

Open the project in VS Code with the PlatformIO extension installed, then build the firmware.

```bash
pio run
```

If you want to upload to real hardware, use the usual PlatformIO upload flow for the selected environment.

### Wokwi

This project is set up for Wokwi through `diagram.json` and `wokwi.toml`. The diagram has been arranged manually to keep the OLED, buzzer, servo, and buttons readable in the simulator.

## Dependencies

The project uses these main libraries:

- Adafruit SSD1306
- Adafruit GFX Library
- ESP32Servo

They are declared in `platformio.ini`.

## Implementation Overview

- `src/main.cpp` owns the display, menu system, button scanning, buzzer, and servo state.
- `src/SnakeGame.*` contains the complete Snake gameplay logic.
- `src/TetrisGame.*` contains the complete Tetris gameplay logic.
- `diagram.json` defines the full Wokwi hardware wiring.

The console is intentionally modular so additional games or screens can be added without rewriting the hardware layer.

## Notes

- Buttons are wired active-low and use internal pull-ups.
- The servo lock state is persistent until the user explicitly locks it again.
- The project is designed to work well in Wokwi, but the same pin map also matches the ESP32 board used by PlatformIO.
