# ESP32_Dinojump

![logo](logo.svg)

Chrome T-Rex runner on **ESP32 + SSD1306 128x64 OLED (I2C 0x3C)** with a single push button + buzzer. Includes a START / RECORD / INFO menu and persists the high score to flash.

## Features

- `START` — run and jump over cacti + pterodactyls, collect hearts for extra lives
- `RECORD` — shows `HI` / `LAST` score (survives reboot via Preferences/NVS)
- `INFO` — button help + display spec

## Hardware

| Part | Wiring |
|---|---|
| OLED SSD1306 SDA | GPIO21 |
| OLED SSD1306 SCL | GPIO22 |
| OLED VCC / GND | 3V3 / GND |
| Push button | GPIO18 <-> GND (`INPUT_PULLUP`, pressed = LOW) |
| Buzzer (+) | GPIO12 (optional) |

> Display must be SSD1306 0.96" at address `0x3C`. For SH1106 1.3", comment out `#define LCD_SSD1306` in `t-rex-duino.h`.

## Controls

| Screen | Button |
|---|---|
| Menu | 1 short press = next, long press ~0.5s / double-click = select |
| RECORD / INFO | any press = back to menu |
| In game | hold = jump (`trex.jump()`), release = fall |
| Game Over | short / long press = back to menu (score auto-saved) |

## Build & Flash

1. Open `trex_esp32_pushbutton.ino` in Arduino IDE
2. Select an ESP32 board (e.g. ESP32 Dev Module), `Upload Speed 921600`
3. No OLED library needed — custom I2C driver in `TRexCore.h` (400kHz clock, 32-byte chunks to avoid `Wire` overflow)
4. Open Serial at 115200 for logs

Standalone display test (no library needed):

```cpp
// oled_test — see full code in chat history
Wire.begin(21, 22);
Wire.setClock(100000); // scan for 0x3C
Wire.setClock(400000); // init SSD1306 + white/black/checker test
```

## Project structure

```
trex_esp32_pushbutton.ino  — setup/loop, SDA/SCL/BTN/BUZZER pins
t-rex-duino.h              — sounds, button (debounce/multi-click/long-press),
                             START/RECORD/INFO menu, gameLoop, 5x7 font
TRexCore.h                 — game sprites, engine, SSD1306/SH1106 I2C driver
logo.svg                   — project logo
```

## Useful config

- `t-rex-duino.h: #define LCD_SSD1306` — defined = SSD1306 / undefined = SH1106
- `TRexCore.h: SH1106_I2C_SCL_CLOCK 400000UL` — drop to `100000UL` if display glitches
- `GAME_BUTTON_PIN / BUZZER_PIN` in `.ino`
- `TARGET_FPS_START/MAX`, `LIVES_START/MAX` in `t-rex-duino.h`
- Clear scores: `dinoPrefs.clear()` or flash with erase

## Troubleshooting (black screen)

1. Run an I2C scanner, it must find `0x3C`. If not = wiring/power (swapped SDA/SCL is most common, VCC must be 3V3).
2. Found `0x3C` but black = wrong driver (SH1106 vs SSD1306) or clock too high, try `100kHz`.
3. Splash shows but shifted = wrong addressing mode, must be Page mode (`0x20,0x02`).
4. GPIO12 is a strapping pin — if the board won't boot, disconnect the buzzer and retry.

## Credit

- Codebase / tutorial: [ESP32 Dino Game with OLED Display | Complete Tutorial](https://www.youtube.com/watch?v=MlQA4UohX_I)
- Base game: [AlexIII/t-rex-duino](https://github.com/AlexIII/t-rex-duino) (MIT)
- ESP32 + SSD1306 port, menu/record, I2C chunk fix: this repo
