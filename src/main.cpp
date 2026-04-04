#include <Arduino.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ESP32Servo.h>

#include <array>

// Hardware pins for the ESP32 DevKit V1 / Wokwi wiring.
constexpr uint8_t TFT_CS = 5;
constexpr uint8_t TFT_RST = 4;
constexpr uint8_t TFT_DC = 2;
constexpr uint8_t TFT_MOSI = 23;
constexpr uint8_t TFT_SCK = 18;
constexpr uint8_t TFT_MISO = 19;

constexpr uint8_t BUTTON_UP = 32;
constexpr uint8_t BUTTON_DOWN = 33;
constexpr uint8_t BUTTON_LEFT = 25;
constexpr uint8_t BUTTON_RIGHT = 26;
constexpr uint8_t BUTTON_A = 27;
constexpr uint8_t BUTTON_B = 14;
constexpr uint8_t BUTTON_START = 22;
constexpr uint8_t BUTTON_SELECT = 13;

constexpr uint8_t BUZZER_PIN = 15;
constexpr uint8_t SERVO_PIN = 21;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

enum class Screen {
  MainMenu,
  Settings,
  InputTest,
  PlaceholderGame,
};

enum class MainMenuItem : uint8_t {
  Snake = 0,
  Tetris = 1,
  Settings = 2,
  Count = 3,
};

enum class SettingsItem : uint8_t {
  InputTest = 0,
  ServoTest = 1,
  Back = 2,
  Count = 3,
};

enum class ButtonId : uint8_t {
  Up = 0,
  Down = 1,
  Left = 2,
  Right = 3,
  A = 4,
  B = 5,
  Start = 6,
  Select = 7,
  Count = 8,
};

struct Button {
  uint8_t pin = 0;
  bool stableState = HIGH;
  bool lastRawState = HIGH;
  uint32_t lastTransitionMs = 0;
  bool pressedEdge = false;
  bool releasedEdge = false;

  void begin() {
    pinMode(pin, INPUT_PULLUP);
    stableState = digitalRead(pin);
    lastRawState = stableState;
    lastTransitionMs = millis();
  }

  void update(uint32_t now) {
    pressedEdge = false;
    releasedEdge = false;

    const bool rawState = digitalRead(pin);
    if (rawState != lastRawState) {
      lastRawState = rawState;
      lastTransitionMs = now;
    }

    if ((now - lastTransitionMs) >= 25 && stableState != lastRawState) {
      stableState = lastRawState;
      if (stableState == LOW) {
        pressedEdge = true;
      } else {
        releasedEdge = true;
      }
    }
  }

  bool isPressed() const {
    return stableState == LOW;
  }

  bool justPressed() const {
    return pressedEdge;
  }

  bool justReleased() const {
    return releasedEdge;
  }
};

class Buzzer {
 public:
  explicit Buzzer(uint8_t pin) : pin_(pin) {}

  void begin() const {
    pinMode(pin_, OUTPUT);
    noTone(pin_);
  }

  void click() const {
    tone(pin_, 2200, 30);
  }

  void select() const {
    tone(pin_, 1400, 55);
  }

  void info() const {
    tone(pin_, 1800, 25);
  }

  void error() const {
    tone(pin_, 240, 80);
  }

 private:
  uint8_t pin_;
};

class ServoController {
 public:
  void begin() {
    servo_.setPeriodHertz(50);
    servo_.attach(SERVO_PIN, 500, 2400);
    servo_.write(restAngle_);
  }

  bool update(uint32_t now) {
    if (testActive_ && static_cast<int32_t>(now - testReturnAtMs_) >= 0) {
      servo_.write(restAngle_);
      testActive_ = false;
      return true;
    }

    return false;
  }

  void triggerTest(uint32_t now) {
    if (testActive_) {
      return;
    }

    servo_.write(testAngle_);
    testReturnAtMs_ = now + 320;
    testActive_ = true;
  }

  bool isTesting() const {
    return testActive_;
  }

 private:
  Servo servo_;
  const int restAngle_ = 90;
  const int testAngle_ = 25;
  bool testActive_ = false;
  uint32_t testReturnAtMs_ = 0;
};

const std::array<uint8_t, static_cast<size_t>(ButtonId::Count)> buttonPins = {{
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_A,
  BUTTON_B,
  BUTTON_START,
  BUTTON_SELECT,
}};

const std::array<const char *, static_cast<size_t>(ButtonId::Count)> buttonLabels = {{
    "Sus",
    "Jos",
    "Stanga",
    "Dreapta",
    "A",
    "B",
    "Start",
    "Select",
}};

Buzzer buzzer(BUZZER_PIN);
ServoController servoController;
std::array<Button, static_cast<size_t>(ButtonId::Count)> buttons;

Screen currentScreen = Screen::MainMenu;
MainMenuItem mainMenuSelection = MainMenuItem::Snake;
SettingsItem settingsSelection = SettingsItem::InputTest;
String placeholderTitle = "";
uint8_t inputSeenMask = 0;
bool screenDirty = true;
bool buttonsReady = false;

// Track last button press for real-time feedback
int lastButtonPressed = -1;
uint32_t lastButtonPressedAtMs = 0;

constexpr uint16_t COLOR_BG = 0x08A2;
constexpr uint16_t COLOR_PANEL = 0x2945;
constexpr uint16_t COLOR_PANEL_ALT = 0x31A6;
constexpr uint16_t COLOR_HIGHLIGHT = ILI9341_CYAN;
constexpr uint16_t COLOR_HIGHLIGHT_TEXT = ILI9341_BLACK;
constexpr uint16_t COLOR_TEXT = ILI9341_WHITE;
constexpr uint16_t COLOR_MUTED = 0xBDF7;

int countSeenButtons() {
  int count = 0;
  for (uint8_t index = 0; index < static_cast<uint8_t>(ButtonId::Count); ++index) {
    if (inputSeenMask & (1U << index)) {
      ++count;
    }
  }
  return count;
}

void setScreen(Screen screen) {
  currentScreen = screen;
  screenDirty = true;
}

void selectMainMenuItem(MainMenuItem item) {
  mainMenuSelection = item;
  screenDirty = true;
}

void selectSettingsItem(SettingsItem item) {
  settingsSelection = item;
  screenDirty = true;
}

void drawCenteredText(const char *text, int16_t centerX, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  tft.setTextSize(size);
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(centerX - static_cast<int16_t>(w / 2), y);
  tft.setTextColor(color);
  tft.print(text);
}

void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, bool selected, const char *title, const char *subtitle = nullptr) {
  const uint16_t fillColor = selected ? COLOR_HIGHLIGHT : COLOR_PANEL;
  const uint16_t borderColor = selected ? COLOR_HIGHLIGHT : COLOR_PANEL_ALT;
  const uint16_t titleColor = selected ? COLOR_HIGHLIGHT_TEXT : COLOR_TEXT;
  const uint16_t subtitleColor = selected ? COLOR_HIGHLIGHT_TEXT : COLOR_MUTED;

  tft.fillRoundRect(x, y, w, h, 10, fillColor);
  tft.drawRoundRect(x, y, w, h, 10, borderColor);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(titleColor);
  tft.setCursor(x + 12, y + 12);
  tft.print(title);

  if (subtitle != nullptr) {
    tft.setTextSize(1);
    tft.setTextColor(subtitleColor);
    tft.setCursor(x + 12, y + 42);
    tft.print(subtitle);
  }
}

void drawHeader(const char *title, const char *subtitle = nullptr) {
  tft.fillRect(0, 0, 240, 54, COLOR_PANEL);
  tft.drawFastHLine(0, 54, 240, COLOR_PANEL_ALT);
  tft.setTextWrap(false);
  drawCenteredText(title, 120, 12, 2, COLOR_TEXT);
  if (subtitle != nullptr) {
    drawCenteredText(subtitle, 120, 34, 1, COLOR_MUTED);
  }
}

void drawFooter(const char *text) {
  tft.fillRect(0, 294, 240, 26, COLOR_PANEL);
  tft.drawFastHLine(0, 294, 240, COLOR_PANEL_ALT);
  drawCenteredText(text, 120, 302, 1, COLOR_MUTED);
}

void drawMainMenu() {
  tft.fillScreen(COLOR_BG);
  drawHeader("ESP32 Game Console", "Prototype framework");

  drawCard(24, 74, 192, 48, mainMenuSelection == MainMenuItem::Snake, "Snake", "Not implemented yet");
  drawCard(24, 130, 192, 48, mainMenuSelection == MainMenuItem::Tetris, "Tetris", "Not implemented yet");
  drawCard(24, 186, 192, 48, mainMenuSelection == MainMenuItem::Settings, "Settings", "Hardware tests and info");

  drawFooter("UP/DOWN choose  A or START confirm");
}

void drawSettingsScreen() {
  tft.fillScreen(COLOR_BG);
  drawHeader("Settings", "Project info and hardware tests");

  char statusLine[48];
  snprintf(statusLine, sizeof(statusLine), "Input test status: %d/8 buttons seen", countSeenButtons());

  tft.fillRoundRect(16, 64, 208, 52, 10, COLOR_PANEL);
  tft.drawRoundRect(16, 64, 208, 52, 10, COLOR_PANEL_ALT);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(28, 77);
  tft.print("ESP32 Game Console");
  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED);
  tft.setCursor(28, 97);
  tft.print(statusLine);

  drawCard(24, 132, 192, 48, settingsSelection == SettingsItem::InputTest, "Input Test", "Highlight each button as you press it");
  drawCard(24, 188, 192, 48, settingsSelection == SettingsItem::ServoTest, "Servo Test", "Moves the servo briefly and returns it");
  drawCard(24, 244, 192, 48, settingsSelection == SettingsItem::Back, "Back", "Return to the main menu");

  char servoLine[40];
  snprintf(servoLine, sizeof(servoLine), "Servo status: %s", servoController.isTesting() ? "running" : "ready");
  drawFooter(servoLine);
}

void drawInputTestScreen() {
  tft.fillScreen(COLOR_BG);
  drawHeader("Input Test", "Press each button and watch the highlights");

  const int16_t startX = 14;
  const int16_t startY = 68;
  const int16_t cellW = 100;
  const int16_t cellH = 40;
  const int16_t gapX = 10;
  const int16_t gapY = 10;

  auto drawButtonCell = [&](ButtonId id, int16_t x, int16_t y) {
    const bool pressed = buttons[static_cast<size_t>(id)].isPressed();
    const bool seen = (inputSeenMask & (1U << static_cast<uint8_t>(id))) != 0;
    const uint16_t fillColor = pressed ? COLOR_HIGHLIGHT : (seen ? COLOR_PANEL_ALT : COLOR_PANEL);
    const uint16_t textColor = pressed ? COLOR_HIGHLIGHT_TEXT : COLOR_TEXT;

    tft.fillRoundRect(x, y, cellW, cellH, 8, fillColor);
    tft.drawRoundRect(x, y, cellW, cellH, 8, COLOR_PANEL_ALT);
    tft.setTextColor(textColor);
    tft.setTextSize(2);
    tft.setCursor(x + 12, y + 12);
    tft.print(buttonLabels[static_cast<size_t>(id)]);
  };

  drawButtonCell(ButtonId::Up, startX + cellW + gapX, startY);
  drawButtonCell(ButtonId::Down, startX + cellW + gapX, startY + cellH + gapY);
  drawButtonCell(ButtonId::Left, startX, startY + cellH + gapY);
  drawButtonCell(ButtonId::Right, startX + cellW + gapX, startY + 2 * (cellH + gapY));
  drawButtonCell(ButtonId::A, startX + cellW + gapX, startY + 3 * (cellH + gapY));
  drawButtonCell(ButtonId::B, startX, startY + 3 * (cellH + gapY));
  drawButtonCell(ButtonId::Start, startX + cellW + gapX, startY + 4 * (cellH + gapY));
  drawButtonCell(ButtonId::Select, startX, startY + 4 * (cellH + gapY));

  // Display last button pressed with real-time feedback
  tft.fillRoundRect(30, 240, 180, 48, 10, COLOR_PANEL);
  tft.drawRoundRect(30, 240, 180, 48, 10, COLOR_PANEL_ALT);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_HIGHLIGHT);
  if (lastButtonPressed >= 0 && lastButtonPressed < static_cast<int>(ButtonId::Count)) {
    drawCenteredText(buttonLabels[lastButtonPressed], 120, 252, 2, COLOR_HIGHLIGHT);
  } else {
    drawCenteredText("waiting...", 120, 252, 1, COLOR_MUTED);
  }

  drawFooter("B or SELECT to return");
}

void drawPlaceholderScreen() {
  tft.fillScreen(COLOR_BG);
  drawHeader(placeholderTitle.c_str(), "Game framework placeholder");

  tft.fillRoundRect(20, 92, 200, 98, 12, COLOR_PANEL);
  tft.drawRoundRect(20, 92, 200, 98, 12, COLOR_PANEL_ALT);
  drawCenteredText("Game logic is not", 120, 116, 2, COLOR_TEXT);
  drawCenteredText("implemented yet", 120, 140, 2, COLOR_TEXT);
  drawCenteredText("Press B or SELECT to go back", 120, 168, 1, COLOR_MUTED);

  drawFooter("A/START can confirm from the menu");
}

void renderScreen() {
  switch (currentScreen) {
    case Screen::MainMenu:
      drawMainMenu();
      break;
    case Screen::Settings:
      drawSettingsScreen();
      break;
    case Screen::InputTest:
      drawInputTestScreen();
      break;
    case Screen::PlaceholderGame:
      drawPlaceholderScreen();
      break;
  }

  screenDirty = false;
}

void enterPlaceholderGame(const char *title) {
  placeholderTitle = title;
  setScreen(Screen::PlaceholderGame);
}

void openSelectedMainMenuItem() {
  buzzer.select();
  switch (mainMenuSelection) {
    case MainMenuItem::Snake:
      enterPlaceholderGame("Snake");
      break;
    case MainMenuItem::Tetris:
      enterPlaceholderGame("Tetris");
      break;
    case MainMenuItem::Settings:
      setScreen(Screen::Settings);
      break;
    case MainMenuItem::Count:
      break;
  }
}

void activateSettingsSelection(uint32_t now) {
  switch (settingsSelection) {
    case SettingsItem::InputTest:
      buzzer.info();
      setScreen(Screen::InputTest);
      break;
    case SettingsItem::ServoTest:
      buzzer.select();
      servoController.triggerTest(now);
      screenDirty = true;
      break;
    case SettingsItem::Back:
      buzzer.select();
      setScreen(Screen::MainMenu);
      break;
    case SettingsItem::Count:
      break;
  }
}

void handleMainMenuInput(uint32_t now) {
  if (buttons[static_cast<size_t>(ButtonId::Up)].justPressed()) {
    const uint8_t next = (static_cast<uint8_t>(mainMenuSelection) + static_cast<uint8_t>(MainMenuItem::Count) - 1) % static_cast<uint8_t>(MainMenuItem::Count);
    selectMainMenuItem(static_cast<MainMenuItem>(next));
    buzzer.click();
  }

  if (buttons[static_cast<size_t>(ButtonId::Down)].justPressed()) {
    const uint8_t next = (static_cast<uint8_t>(mainMenuSelection) + 1) % static_cast<uint8_t>(MainMenuItem::Count);
    selectMainMenuItem(static_cast<MainMenuItem>(next));
    buzzer.click();
  }

  if (buttons[static_cast<size_t>(ButtonId::A)].justPressed() || buttons[static_cast<size_t>(ButtonId::Start)].justPressed()) {
    openSelectedMainMenuItem();
  }

  (void)now;
}

void handleSettingsInput(uint32_t now) {
  if (buttons[static_cast<size_t>(ButtonId::Up)].justPressed()) {
    const uint8_t next = (static_cast<uint8_t>(settingsSelection) + static_cast<uint8_t>(SettingsItem::Count) - 1) % static_cast<uint8_t>(SettingsItem::Count);
    selectSettingsItem(static_cast<SettingsItem>(next));
    buzzer.click();
  }

  if (buttons[static_cast<size_t>(ButtonId::Down)].justPressed()) {
    const uint8_t next = (static_cast<uint8_t>(settingsSelection) + 1) % static_cast<uint8_t>(SettingsItem::Count);
    selectSettingsItem(static_cast<SettingsItem>(next));
    buzzer.click();
  }

  if (buttons[static_cast<size_t>(ButtonId::A)].justPressed() || buttons[static_cast<size_t>(ButtonId::Start)].justPressed()) {
    activateSettingsSelection(now);
  }

  if (buttons[static_cast<size_t>(ButtonId::B)].justPressed() || buttons[static_cast<size_t>(ButtonId::Select)].justPressed()) {
    buzzer.select();
    setScreen(Screen::MainMenu);
  }
}

void handleInputTestInput(uint32_t now) {
  for (uint8_t index = 0; index < static_cast<uint8_t>(ButtonId::Count); ++index) {
    if (buttons[index].justPressed()) {
      inputSeenMask |= (1U << index);
      lastButtonPressed = index;
      lastButtonPressedAtMs = now;
      buzzer.click();
      screenDirty = true;
    }
  }

  if (buttons[static_cast<size_t>(ButtonId::B)].justPressed() || buttons[static_cast<size_t>(ButtonId::Select)].justPressed()) {
    buzzer.select();
    setScreen(Screen::Settings);
  }
}

void handlePlaceholderInput() {
  if (buttons[static_cast<size_t>(ButtonId::B)].justPressed() || buttons[static_cast<size_t>(ButtonId::Select)].justPressed()) {
    buzzer.select();
    setScreen(Screen::MainMenu);
  }
}

void updateButtons(uint32_t now) {
  for (Button &button : buttons) {
    button.update(now);
  }
}

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(0);

  buzzer.begin();
  servoController.begin();

  for (size_t index = 0; index < buttons.size(); ++index) {
    buttons[index].pin = buttonPins[index];
  }

  for (Button &button : buttons) {
    button.begin();
  }
  buttonsReady = true;

  tft.fillScreen(COLOR_BG);
  drawCenteredText("Booting console...", 120, 146, 2, COLOR_TEXT);
  delay(150);
  screenDirty = true;
}

void loop() {
  const uint32_t now = millis();
  updateButtons(now);
  if (servoController.update(now)) {
    screenDirty = true;
  }

  if (buttonsReady) {
    switch (currentScreen) {
      case Screen::MainMenu:
        handleMainMenuInput(now);
        break;
      case Screen::Settings:
        handleSettingsInput(now);
        break;
      case Screen::InputTest:
        handleInputTestInput(now);
        break;
      case Screen::PlaceholderGame:
        handlePlaceholderInput();
        break;
    }
  }

  if (screenDirty) {
    renderScreen();
  }
}