/*
 * Project name: T-rex-duino
 * Description: T-rex game from Chrome browser rewritten for Arduino
 * Standalone adaptation for ESP01 / Arduino
 */

#include <Wire.h>
#if defined(ESP32)
#include <Preferences.h>
#endif

#ifndef BUTTON_ACTIVE_HIGH
#define BUTTON_ACTIVE_HIGH 0
#endif

static inline bool trexButtonPressed() {
#if BUTTON_ACTIVE_HIGH
  return digitalRead(TOUCH_SENSOR_PIN) == HIGH;
#else
  return digitalRead(TOUCH_SENSOR_PIN) == LOW;
#endif
}

/* Buzzer types */
struct BuzzerNote {
  int freq;
  int dur;
};

/* Sound effects */
const BuzzerNote SND_MENU_BACK[] = {
  { 2400, 30 }, { 0, 15 }, { 1800, 40 }
};

const BuzzerNote SND_DINO_JUMP[] = {
  { 1000, 8 }, { 0, 4 }, { 1400, 8 }, { 0, 4 }, { 1800, 12 }
};

const BuzzerNote SND_DINO_HIT[] = {
  { 900, 25 }, { 0, 10 }, { 700, 35 }
};

const BuzzerNote SND_DINO_GAMEOVER[] = {
  { 800, 60 }, { 0, 15 }, { 600, 60 }, { 0, 15 }, { 400, 60 }, { 0, 15 }, { 300, 100 }
};

const BuzzerNote SND_DINO_HEART[] = {
  { 1100, 10 }, { 0, 5 }, { 1400, 10 }, { 0, 5 }, { 1700, 14 }
};

const BuzzerNote SND_MENU_MOVE[] = {
  { 1500, 20 }
};

const BuzzerNote SND_MENU_SELECT[] = {
  { 1800, 25 }, { 0, 10 }, { 2200, 35 }
};

/* High score record (persisted on ESP32 NVS) */
static uint16_t dinoHiScore = 0;
static uint16_t dinoLastScore = 0;
#if defined(ESP32)
static Preferences dinoPrefs;
static bool dinoPrefsBegun = false;
#endif

static void dinoLoadRecord() {
#if defined(ESP32)
  if (!dinoPrefsBegun) { dinoPrefs.begin("dino", false); dinoPrefsBegun = true; }
  dinoHiScore = dinoPrefs.getUShort("hi", 0);
  dinoLastScore = dinoPrefs.getUShort("last", 0);
#endif
}

static void dinoSaveRecord() {
#if defined(ESP32)
  if (!dinoPrefsBegun) { dinoPrefs.begin("dino", false); dinoPrefsBegun = true; }
  dinoPrefs.putUShort("hi", dinoHiScore);
  dinoPrefs.putUShort("last", dinoLastScore);
#endif
}

#ifndef PLAY
#define PLAY(s) buzzer_play(s, sizeof(s) / sizeof(s[0]))
#endif

/* Buzzer engine */
static const BuzzerNote* _seq = nullptr;
static int _seqLen = 0;
static int _noteIdx = 0;
static unsigned long _noteStart = 0;
static bool _buzPlaying = false;

void buzzer_update() {
  if (!_buzPlaying) return;
  unsigned long now = millis();
  if (now - _noteStart < (unsigned long)_seq[_noteIdx].dur) return;
  _noteIdx++;
  if (_noteIdx >= _seqLen) {
    noTone(BUZZER_PIN);
    _buzPlaying = false;
    return;
  }
  _noteStart = now;
  (_seq[_noteIdx].freq == 0) ? noTone(BUZZER_PIN)
                             : tone(BUZZER_PIN, _seq[_noteIdx].freq);
}

void buzzer_play(const BuzzerNote* seq, int len) {
  _seq = seq;
  _seqLen = len;
  _noteIdx = 0;
  _noteStart = millis();
  _buzPlaying = true;
  (seq[0].freq == 0) ? noTone(BUZZER_PIN) : tone(BUZZER_PIN, seq[0].freq);
}

/* Touch/click state */
bool singleClick = false;
bool doubleClick = false;
bool tripleClick = false;
bool quadClick = false;
bool longPress = false;

static bool _stableState = LOW;
static bool _lastReading = LOW;
static bool _longPressFired = false;
static bool _multiClickWaiting = false;
static uint8_t _clickCount = 0;
static unsigned long _lastDebounceTime = 0;
static unsigned long _pressStartTime = 0;
static unsigned long _lastReleaseTime = 0;

#define DEBOUNCE_TIME 20
#define MULTI_CLICK_GAP 250
#define LONG_PRESS_TIME 500

void updateTouch() {
  singleClick = false;
  doubleClick = false;
  tripleClick = false;
  quadClick = false;
  longPress = false;

  bool reading = trexButtonPressed();

  if (reading != _lastReading) {
    _lastDebounceTime = millis();
    _lastReading = reading;
  }

  if ((millis() - _lastDebounceTime) > DEBOUNCE_TIME) {
    if (reading != _stableState) {
      _stableState = reading;
      if (_stableState == HIGH) {
        _pressStartTime = millis();
        _longPressFired = false;
        if ((millis() - _lastReleaseTime) > 2000) {
          _clickCount = 0;
          _multiClickWaiting = false;
        }
      } else {
        if (!_longPressFired) {
          _clickCount++;
          _multiClickWaiting = true;
          _lastReleaseTime = millis();
        }
      }
    }
  }

  if (_stableState == HIGH && !_longPressFired) {
    if ((millis() - _pressStartTime) >= LONG_PRESS_TIME) {
      _longPressFired = true;
      _clickCount = 0;
      _multiClickWaiting = false;
      longPress = true;
    }
  }

  if (_multiClickWaiting && _stableState == LOW) {
    if ((millis() - _lastReleaseTime) >= MULTI_CLICK_GAP) {
      _multiClickWaiting = false;
      uint8_t clicks = _clickCount;
      _clickCount = 0;
      if (clicks == 1) singleClick = true;
      else if (clicks == 2) doubleClick = true;
      else if (clicks == 3) tripleClick = true;
      else if (clicks == 4) quadClick = true;
    }
  }
}

void clearTouchState() {
  singleClick = false;
  doubleClick = false;
  tripleClick = false;
  quadClick = false;
  longPress = false;
  bool currentRead = trexButtonPressed();
  _stableState = currentRead;
  _lastReading = currentRead;
  _longPressFired = true;
  _multiClickWaiting = false;
  _clickCount = 0;
}

// -- Display Selection -- SSD1306 0.96" OLED
#define LCD_SSD1306   // uncommented for SSD1306
#define LCD_HEIGHT 64U
#define LCD_WIDTH 128U
#define VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS 4

/* Misc. Settings */
#define PLAYER_SAFE_ZONE_WIDTH 32
#define CACTI_RESPAWN_RATE 50
#define GROUND_CACTI_SCROLL_SPEED 3
#define PTERODACTY_SPEED 5
#define PTERODACTY_RESPAWN_RATE 255
#define INCREASE_FPS_EVERY_N_SCORE_POINTS 256
#define LIVES_START 3
#define LIVES_MAX 5
#define SPAWN_NEW_LIVE_MIN_CYCLES 800
#define DAY_NIGHT_SWITCH_CYCLES 1024
#define TARGET_FPS_START 23
#define TARGET_FPS_MAX 48

/* Includes */
#include "TRexCore.h"
using namespace TRexGame;

/* Defines and globals */
#define LCD_BYTE_SZIE (LCD_WIDTH * LCD_HEIGHT / 8)
#define LCD_IF_VIRTUAL_WIDTH(TRUE_COND, FALSE_COND) FALSE_COND
#define LCD_PART_BUFF_WIDTH LCD_WIDTH
#define LCD_PART_BUFF_HEIGHT (VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS * 8)
#define LCD_PART_BUFF_SZ ((LCD_PART_BUFF_HEIGHT / 8) * LCD_PART_BUFF_WIDTH)

I2C i2c;
SH1106<I2C> lcd(i2c, LCD_BYTE_SZIE);

bool dinoFirstRun = true;

/* Misc Functions */
bool isPressedJump() { return trexButtonPressed(); }

bool isPressedDuck() { return false; }

uint8_t randByte() {
  static uint16_t c = 0xA7E2;
  c = (c << 1) | (c >> 15);
  c = (c << 1) | (c >> 15);
  c = (c << 1) | (c >> 15);
#if defined(ESP8266) || defined(ESP32)
  c = analogRead(A0) ^ ESP.getCycleCount() ^ c;
#else
  c = analogRead(A2) ^ analogRead(A3) ^ analogRead(A4) ^ analogRead(A5) ^
      analogRead(A6) ^ analogRead(A7) ^ c;
#endif
  return c;
}

/* Main Functions */
struct Renderer {
  static void renderNumber(BitCanvas &canvas, Point2Di8 point,
                           const uint16_t number) {
    uint16_t base = 10000;
    while (base) {
      const uint8_t digit = (number / base) % 10;
      canvas.render(numbers.getSprite(digit, point));
      base /= 10;
      point.x += numbers.getWidth() + 1;
    }
  }
};

void gameLoop() {
  uint8_t lcdBuff[LCD_PART_BUFF_SZ];
  VirtualBitCanvas bitCanvas(
      LCD_IF_VIRTUAL_WIDTH(VirtualBitCanvas::VIRTUAL_WIDTH,
                           VirtualBitCanvas::VIRTUAL_HEIGHT),
      lcdBuff, LCD_PART_BUFF_HEIGHT, LCD_PART_BUFF_WIDTH,
      LCD_IF_VIRTUAL_WIDTH(LCD_WIDTH, LCD_HEIGHT));

  SpawnHold spawnHolder;

  // dynamic sprites
  TrexPlayer trex;
  Ground ground1(-1);
  Ground ground2(63);
  Ground ground3(127);
  Cactus cactus1(spawnHolder);
  Cactus cactus2(spawnHolder);
  Pterodactyl pterodactyl1(spawnHolder);
  HeartLive heartLive;
  const array<SpriteAnimated *, 8> sprites{{&ground1, &ground2, &ground3,
                                             &cactus1, &cactus2, &pterodactyl1,
                                             &heartLive, &trex}};
  const array<SpriteAnimated *, 3> enemies{{&cactus1, &cactus2, &pterodactyl1}};

  // static sprites
  const Sprite gameOverSprite(&game_overver_bm, {15, 12});
  const Sprite restartIconSprite(&restart_icon_bm, {55, 25});
  Sprite heartsSprite(&hearts_5x_bm, {95, 8});

  // game variables
  uint32_t prvT = 0;
  bool gameOver = false;
  uint16_t score = 0;
  uint8_t targetFPS = TARGET_FPS_START;
  uint8_t lives = LIVES_START;
  bool night = false;
  bool jumpSoundPlayed = false;
  lcd.setInverse(night);

  // main cycle
  while (1) {
    // background tasks
    updateTouch();
    buzzer_update();

    // render cycle
    while (1) {
      // score
      Renderer::renderNumber(bitCanvas, {95, 0}, score);
      bitCanvas.render(heartsSprite);
      // game objects
      for (uint8_t i = 0; i < sprites.size(); ++i)
        bitCanvas.render(*sprites[i]);
      // game over
      if (gameOver) {
        bitCanvas.render(gameOverSprite);
        bitCanvas.render(restartIconSprite);
      }
      // update screen
      lcd.fillScreen(lcdBuff, LCD_PART_BUFF_SZ,
                     LCD_IF_VIRTUAL_WIDTH(LCD_PART_BUFF_WIDTH, 0));
      if (bitCanvas.nextPart())
        break;
    }

    // exit game on game over
    if (gameOver) {
      delay(500);
      clearTouchState();

      while (1) {
        updateTouch();
        buzzer_update();

        if (singleClick || longPress) {
          singleClick = false;
          longPress = false;
          PLAY(SND_MENU_BACK);
          clearTouchState();
          return;
        }
        yield();
      }
    }

    // collision detection
    if (!trex.isBlinking() &&
        CollisionDetector::check(trex, enemies.data, enemies.size())) {
      if (lives) {
        trex.blink();
        --lives;
        PLAY(SND_DINO_HIT);
      } else {
        trex.die();
        gameOver = true;
        dinoLastScore = score;
        if (score > dinoHiScore) dinoHiScore = score;
        dinoSaveRecord();
        PLAY(SND_DINO_GAMEOVER);
        continue;
      }
    }
    if (lives < LIVES_MAX && CollisionDetector::check(trex, heartLive)) {
      ++lives;
      heartLive.eat();
      PLAY(SND_DINO_HEART);
    }

    // controls
    if (isPressedJump()) {
      if (!jumpSoundPlayed && trex.state != TrexPlayer::DEAD) {
        PLAY(SND_DINO_JUMP);
        jumpSoundPlayed = true;
      }
      trex.jump();
    } else {
      jumpSoundPlayed = false;
    }
    trex.duck(isPressedDuck());

    // logic and animation step
    for (uint8_t i = 0; i < sprites.size(); ++i)
      sprites[i]->step();
    // score keeping
    if (score < 0xFFFE)
      ++score;
    // make game progressively faster
    if (!(score % INCREASE_FPS_EVERY_N_SCORE_POINTS) &&
        targetFPS < TARGET_FPS_MAX)
      ++targetFPS;
    heartsSprite.limitRenderWidthTo = 6 * lives + 1;
    // switch day and night
    if (!(score % DAY_NIGHT_SWITCH_CYCLES))
      lcd.setInverse(night = !night);

    const uint8_t frameTime = 1000 / targetFPS;

    // throttle
    while (millis() - prvT < frameTime) {
      updateTouch();
      buzzer_update();
#if defined(ESP8266) || defined(ESP32)
      yield();
#endif
    }
    prvT = millis();
  }
}


static void splashSetPixel(uint8_t* buffer, int x, int y) {
  if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;
  buffer[(y / 8) * LCD_WIDTH + x] |= (1 << (y & 7));
}

static uint8_t splashFontColumn(char c, uint8_t col) {
  // 5x7 uppercase font, column encoded, bit 0 = top pixel
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  static const uint8_t B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
  static const uint8_t E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t G[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
  static const uint8_t H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
  static const uint8_t I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
  static const uint8_t J[5] = {0x20, 0x40, 0x41, 0x3F, 0x01};
  static const uint8_t K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
  static const uint8_t L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
  static const uint8_t M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
  static const uint8_t N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
  static const uint8_t O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t Q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
  static const uint8_t R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
  static const uint8_t W[5] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
  static const uint8_t X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
  static const uint8_t Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
  static const uint8_t Z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};
  static const uint8_t D0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t D1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t D2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t D3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
  static const uint8_t D4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t D5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t D6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t D7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t D8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t D9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t COLON[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t GT[5] = {0x08, 0x04, 0x02, 0x04, 0x08};
  static const uint8_t DOT[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  const uint8_t* glyph = blank;
  switch (c) {
    case 'A': glyph = A; break;
    case 'B': glyph = B; break;
    case 'C': glyph = C; break;
    case 'D': glyph = D; break;
    case 'E': glyph = E; break;
    case 'F': glyph = F; break;
    case 'G': glyph = G; break;
    case 'H': glyph = H; break;
    case 'I': glyph = I; break;
    case 'J': glyph = J; break;
    case 'K': glyph = K; break;
    case 'L': glyph = L; break;
    case 'M': glyph = M; break;
    case 'N': glyph = N; break;
    case 'O': glyph = O; break;
    case 'P': glyph = P; break;
    case 'Q': glyph = Q; break;
    case 'R': glyph = R; break;
    case 'S': glyph = S; break;
    case 'T': glyph = T; break;
    case 'U': glyph = U; break;
    case 'V': glyph = V; break;
    case 'W': glyph = W; break;
    case 'X': glyph = X; break;
    case 'Y': glyph = Y; break;
    case 'Z': glyph = Z; break;
    case '0': glyph = D0; break;
    case '1': glyph = D1; break;
    case '2': glyph = D2; break;
    case '3': glyph = D3; break;
    case '4': glyph = D4; break;
    case '5': glyph = D5; break;
    case '6': glyph = D6; break;
    case '7': glyph = D7; break;
    case '8': glyph = D8; break;
    case '9': glyph = D9; break;
    case '-': glyph = DASH; break;
    case ':': glyph = COLON; break;
    case '>': glyph = GT; break;
    case '.': glyph = DOT; break;
    default: glyph = blank; break;
  }
  return glyph[col];
}

static void splashDrawChar(uint8_t* buffer, int x, int y, char c, uint8_t scale) {
  for (uint8_t col = 0; col < 5; ++col) {
    uint8_t bits = splashFontColumn(c, col);
    for (uint8_t row = 0; row < 7; ++row) {
      if (bits & (1 << row)) {
        for (uint8_t dx = 0; dx < scale; ++dx) {
          for (uint8_t dy = 0; dy < scale; ++dy) {
            splashSetPixel(buffer, x + col * scale + dx, y + row * scale + dy);
          }
        }
      }
    }
  }
}

static void splashDrawText(uint8_t* buffer, int x, int y, const char* text, uint8_t scale) {
  while (*text) {
    splashDrawChar(buffer, x, y, *text, scale);
    x += 6 * scale;
    ++text;
  }
}

static void splashDrawHLine(uint8_t* buffer, int x, int y, int w) {
  for (int i = 0; i < w; ++i) splashSetPixel(buffer, x + i, y);
}

void spalshScreen() {
  lcd.setAddressingMode(lcd.HorizontalAddressingMode);

  uint8_t lcdBuff[LCD_BYTE_SZIE];
  BitCanvas canvas(lcdBuff, LCD_HEIGHT, LCD_WIDTH);
  canvas.clear();

  // Clean custom start page. No GitHub/author text on screen.
  splashDrawText(lcdBuff, 37, 4, "ROBO ARMY", 1);
  splashDrawText(lcdBuff, 40, 15, "DINO RUN", 1);
  splashDrawText(lcdBuff, 28, 55, "PRESS BUTTON", 1);

  Sprite splashTrex(&trex_up_1, {25, 51}, Sprite::ANCHOR_BOTTOM_LEFT);
  Sprite splashCactus(&cacti_3s, {82, 51}, Sprite::ANCHOR_BOTTOM_LEFT);
  canvas.render(splashTrex);
  canvas.render(splashCactus);

  splashDrawHLine(lcdBuff, 0, 52, 128);
  for (uint8_t x = 0; x < 128; x += 13) {
    splashSetPixel(lcdBuff, x, 55);
    splashSetPixel(lcdBuff, x + 5, 57);
  }

  lcd.fillScreen(lcdBuff, LCD_BYTE_SZIE);

  while (isPressedJump()) {
    yield();
  }

  clearTouchState();

  while (1) {
    updateTouch();
    buzzer_update();
    if (singleClick || doubleClick || isPressedJump()) {
      clearTouchState();
      break;
    }
    yield();
  }
}

static void splashDrawNumber(uint8_t* buffer, int x, int y, uint16_t value) {
  char tmp[6];
  uint8_t len = 0;
  if (value == 0) { splashDrawChar(buffer, x, y, '0', 1); return; }
  while (value && len < sizeof(tmp)) { tmp[len++] = '0' + (value % 10); value /= 10; }
  for (int8_t i = len - 1; i >= 0; --i) {
    splashDrawChar(buffer, x, y, tmp[i], 1);
    x += 6;
  }
}

static void menuRender(uint8_t sel) {
  uint8_t lcdBuff[LCD_BYTE_SZIE];
  BitCanvas canvas(lcdBuff, LCD_HEIGHT, LCD_WIDTH);
  canvas.clear();

  splashDrawText(lcdBuff, 37, 2, "ROBO ARMY", 1);
  splashDrawText(lcdBuff, 40, 12, "DINO RUN", 1);
  splashDrawHLine(lcdBuff, 0, 22, 128);

  const char* items[3] = {"START", "RECORD", "INFO"};
  for (uint8_t i = 0; i < 3; i++) {
    int y = 28 + i * 11;
    if (i == sel) splashDrawText(lcdBuff, 30, y, ">", 1);
    splashDrawText(lcdBuff, 42, y, items[i], 1);
  }
  splashDrawText(lcdBuff, 14, 55, "1X NEXT LONG OK", 1);

  lcd.fillScreen(lcdBuff, LCD_BYTE_SZIE);
}

// 0=START 1=RECORD 2=INFO. 1 click = next, long-press/double-click = select.
static uint8_t menuLoop() {
  uint8_t sel = 0;
  while (isPressedJump()) yield();
  clearTouchState();
  menuRender(sel);
  while (1) {
    updateTouch();
    buzzer_update();
    if (singleClick) {
      singleClick = false;
      sel = (sel + 1) % 3;
      PLAY(SND_MENU_MOVE);
      menuRender(sel);
    }
    if (longPress || doubleClick) {
      longPress = false; doubleClick = false;
      singleClick = false;
      PLAY(SND_MENU_SELECT);
      clearTouchState();
      while (isPressedJump()) { yield(); buzzer_update(); }
      delay(120);
      return sel;
    }
    yield();
  }
}

static void recordScreen() {
  uint8_t lcdBuff[LCD_BYTE_SZIE];
  BitCanvas canvas(lcdBuff, LCD_HEIGHT, LCD_WIDTH);
  canvas.clear();
  splashDrawText(lcdBuff, 40, 4, "RECORD", 1);
  splashDrawHLine(lcdBuff, 0, 14, 128);
  splashDrawText(lcdBuff, 20, 22, "HI", 1);
  splashDrawNumber(lcdBuff, 62, 22, dinoHiScore);
  splashDrawText(lcdBuff, 20, 34, "LAST", 1);
  splashDrawNumber(lcdBuff, 62, 34, dinoLastScore);
  if (dinoHiScore == 0) splashDrawText(lcdBuff, 20, 46, "NO DATA", 1);
  splashDrawText(lcdBuff, 28, 56, "BACK", 1);
  lcd.fillScreen(lcdBuff, LCD_BYTE_SZIE);

  while (isPressedJump()) yield();
  clearTouchState();
  while (1) {
    updateTouch();
    buzzer_update();
    if (singleClick || doubleClick || longPress || isPressedJump()) {
      PLAY(SND_MENU_BACK);
      clearTouchState();
      while (isPressedJump()) yield();
      delay(120);
      return;
    }
    yield();
  }
}

static void infoScreen() {
  uint8_t lcdBuff[LCD_BYTE_SZIE];
  BitCanvas canvas(lcdBuff, LCD_HEIGHT, LCD_WIDTH);
  canvas.clear();
  splashDrawText(lcdBuff, 48, 4, "INFO", 1);
  splashDrawHLine(lcdBuff, 0, 14, 128);
  splashDrawText(lcdBuff, 8, 20, "BTN 18 HOLD JUMP", 1);
  splashDrawText(lcdBuff, 8, 30, "1X NEXT LONG OK", 1);
  splashDrawText(lcdBuff, 8, 40, "OLED SSD1306 I2C", 1);
  splashDrawText(lcdBuff, 28, 56, "BACK", 1);
  lcd.fillScreen(lcdBuff, LCD_BYTE_SZIE);

  while (isPressedJump()) yield();
  clearTouchState();
  while (1) {
    updateTouch();
    buzzer_update();
    if (singleClick || doubleClick || longPress || isPressedJump()) {
      PLAY(SND_MENU_BACK);
      clearTouchState();
      while (isPressedJump()) yield();
      delay(120);
      return;
    }
    yield();
  }
}

void dino_enter() {
  dinoFirstRun = true;
  clearTouchState();
}

void runDinoGame() {
  if (dinoFirstRun) {
    lcd.begin();
    lcd.setInverse(false);
    dinoLoadRecord();
    lcd.setAddressingMode(LCD_IF_VIRTUAL_WIDTH(lcd.VerticalAddressingMode,
                                               lcd.HorizontalAddressingMode));
    srand((randByte() << 8) | randByte());
    dinoFirstRun = false;
  }

  uint8_t choice = menuLoop();
  if (choice == 0) {
    gameLoop(); // returns here on game-over press -> back to menu on next loop()
  } else if (choice == 1) {
    recordScreen();
  } else {
    infoScreen();
  }
}
