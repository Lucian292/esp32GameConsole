#include <Arduino.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

#include <array>
#include <cstring>

constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 64;

constexpr uint8_t BUTTON_UP = 32;
constexpr uint8_t BUTTON_DOWN = 33;
constexpr uint8_t BUTTON_LEFT = 25;
constexpr uint8_t BUTTON_RIGHT = 26;
constexpr uint8_t BUTTON_A = 27;
constexpr uint8_t BUTTON_B = 14;
constexpr uint8_t BUTTON_START = 18;
constexpr uint8_t BUTTON_SELECT = 19;

constexpr uint8_t BUZZER_PIN = 15;
constexpr uint8_t SERVO_PIN = 13;

constexpr uint8_t SERVO_REST_ANGLE = 90;
constexpr uint8_t SERVO_UNLOCK_ANGLE = 25;
constexpr uint32_t SERVO_TEST_DURATION_MS = 2000;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

enum class Screen : uint8_t {
  MainMenu,
  InputTest,
  Settings,
  Placeholder,
};

enum class MainMenuItem : uint8_t {
  Snake = 0,
  Tetris = 1,
  Settings = 2,
  Count = 3,
};

enum class SettingsItem : uint8_t {
  ServoTest = 0,
  BuzzerTest = 1,
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
  uint32_t lastChangeMs = 0;
  bool justPressedEdge = false;

  void begin(uint8_t assignedPin) {
    pin = assignedPin;
    pinMode(pin, INPUT_PULLUP);
    stableState = digitalRead(pin);
    lastRawState = stableState;
    lastChangeMs = millis();
  }

  void update(uint32_t now) {
    justPressedEdge = false;
    const bool rawState = digitalRead(pin);
    if (rawState != lastRawState) {
      lastRawState = rawState;
      lastChangeMs = now;
    }

    if ((now - lastChangeMs) >= 25 && stableState != lastRawState) {
      stableState = lastRawState;
      if (stableState == LOW) {
        justPressedEdge = true;
      }
    }
  }

  bool pressed() const {
    return stableState == LOW;
  }

  bool justPressed() const {
    return justPressedEdge;
  }
};

class Buzzer {
 public:
  explicit Buzzer(uint8_t pin) : pin_(pin) {}

  void begin() const {
    pinMode(pin_, OUTPUT);
    noTone(pin_);
  }

  void navBeep() const {
    tone(pin_, 2200, 25);
  }

  void confirmBeep() const {
    tone(pin_, 1400, 45);
  }

  void errorBeep() const {
    tone(pin_, 300, 90);
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

  void startTest(uint32_t now) {
    if (running_) {
      return;
    }
    servo_.write(unlockAngle_);
    running_ = true;
    endsAtMs_ = now + SERVO_TEST_DURATION_MS;
  }

  bool update(uint32_t now) {
    if (running_ && static_cast<int32_t>(now - endsAtMs_) >= 0) {
      servo_.write(restAngle_);
      running_ = false;
      return true;
    }
    return false;
  }

  bool isRunning() const {
    return running_;
  }

 private:
  Servo servo_;
  const uint8_t restAngle_ = SERVO_REST_ANGLE;
  const uint8_t unlockAngle_ = SERVO_UNLOCK_ANGLE;
  bool running_ = false;
  uint32_t endsAtMs_ = 0;
};

constexpr std::array<uint8_t, static_cast<size_t>(ButtonId::Count)> buttonPins = {{
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_A,
    BUTTON_B,
    BUTTON_START,
    BUTTON_SELECT,
}};

constexpr std::array<const char *, static_cast<size_t>(ButtonId::Count)> buttonLabels = {{
    "UP",
    "DOWN",
    "LEFT",
    "RIGHT",
    "A",
    "B",
    "START",
    "SELECT",
}};

constexpr std::array<const char *, static_cast<size_t>(MainMenuItem::Count)> mainMenuLabels = {{
    "Snake",
    "Tetris",
    "Settings",
}};

constexpr std::array<const char *, static_cast<size_t>(SettingsItem::Count)> settingsLabels = {{
    "Servo test",
    "Buzzer test",
    "Back",
}};

Buzzer buzzer(BUZZER_PIN);
ServoController servoController;
std::array<Button, static_cast<size_t>(ButtonId::Count)> buttons;

Screen currentScreen = Screen::MainMenu;
MainMenuItem mainMenuSelection = MainMenuItem::Snake;
SettingsItem settingsSelection = SettingsItem::ServoTest;
uint8_t currentPressedMask = 0;
uint8_t renderedPressedMask = 0;
uint8_t lastPressedButton = 255;
bool screenDirty = true;
bool buzzerTestActive = false;
uint32_t buzzerTestEndsAtMs = 0;

void setScreen(Screen screen) {
  currentScreen = screen;
  screenDirty = true;
}

void drawCenteredText(int16_t centerX, int16_t y, const char *text, uint8_t size = 1) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(centerX - static_cast<int16_t>(w / 2), y);
  display.print(text);
}

void buildPressedList(char *buffer, size_t bufferSize) {
  buffer[0] = '\0';
  bool first = true;
  for (uint8_t index = 0; index < static_cast<uint8_t>(ButtonId::Count); ++index) {
    if ((currentPressedMask & (1U << index)) == 0) {
      continue;
    }

    if (!first) {
      strncat(buffer, ", ", bufferSize - strlen(buffer) - 1);
    }
    strncat(buffer, buttonLabels[index], bufferSize - strlen(buffer) - 1);
    first = false;
  }

  if (first) {
    strlcpy(buffer, "None", bufferSize);
  }
}

void drawMainMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("ESP32 Game Console");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  const int itemCount = static_cast<int>(MainMenuItem::Count);
  int startIndex = static_cast<int>(mainMenuSelection) - 1;
  if (startIndex < 0) {
    startIndex = 0;
  }
  if (startIndex > itemCount - 3) {
    startIndex = itemCount - 3;
  }

  for (int row = 0; row < 3; ++row) {
    const int itemIndex = startIndex + row;
    if (itemIndex >= itemCount) {
      continue;
    }
    display.setCursor(0, 16 + row * 12);
    display.print(itemIndex == static_cast<int>(mainMenuSelection) ? "> " : "  ");
    display.print(mainMenuLabels[itemIndex]);
  }

  display.setCursor(0, 52);
  display.print("A/START ok  B/SEL back");
  display.display();
}

void drawInputTest() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Input Test");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  char pressedLine[64];
  buildPressedList(pressedLine, sizeof(pressedLine));

  display.setCursor(0, 16);
  display.print("Pressed:");
  display.setCursor(0, 26);
  display.print(pressedLine);

  display.setCursor(0, 40);
  display.print("Hold buttons to test");
  display.setCursor(0, 52);
  display.print("B/SELECT back");
  display.display();
}

void drawSettings() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Settings");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display.setCursor(0, 14);
  display.print("ESP32 Game Console");

  display.setCursor(0, 24);
  display.print("Servo: ");
  display.print(servoController.isRunning() ? "running" : "ready");

  display.setCursor(0, 36);
  display.print(settingsSelection == SettingsItem::ServoTest ? "> " : "  ");
  display.print(settingsLabels[static_cast<size_t>(SettingsItem::ServoTest)]);

  display.setCursor(0, 46);
  display.print(settingsSelection == SettingsItem::BuzzerTest ? "> " : "  ");
  display.print(settingsLabels[static_cast<size_t>(SettingsItem::BuzzerTest)]);

  display.setCursor(0, 56);
  display.print(settingsSelection == SettingsItem::Back ? "> " : "  ");
  display.print(settingsLabels[static_cast<size_t>(SettingsItem::Back)]);
  display.display();
}

void drawPlaceholder(const char *title) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print("Not implemented yet");
  display.setCursor(0, 34);
  display.print("Press B/SELECT back");
  display.display();
}

void renderScreen() {
  switch (currentScreen) {
    case Screen::MainMenu:
      drawMainMenu();
      break;
    case Screen::InputTest:
      drawInputTest();
      break;
    case Screen::Settings:
      drawSettings();
      break;
    case Screen::Placeholder:
      drawPlaceholder(mainMenuLabels[static_cast<size_t>(mainMenuSelection)]);
      break;
  }
  screenDirty = false;
  renderedPressedMask = currentPressedMask;
}

void openMainMenuSelection() {
  if (mainMenuSelection == MainMenuItem::Settings) {
    buzzer.confirmBeep();
    setScreen(Screen::Settings);
    return;
  }

  buzzer.confirmBeep();
  setScreen(Screen::Placeholder);
}

void handleMainMenuInput(uint32_t now) {
  (void)now;

  if (buttons[static_cast<size_t>(ButtonId::Up)].justPressed()) {
    const int current = static_cast<int>(mainMenuSelection);
    mainMenuSelection = static_cast<MainMenuItem>((current + static_cast<int>(MainMenuItem::Count) - 1) % static_cast<int>(MainMenuItem::Count));
    buzzer.navBeep();
    screenDirty = true;
  }

  if (buttons[static_cast<size_t>(ButtonId::Down)].justPressed()) {
    const int current = static_cast<int>(mainMenuSelection);
    mainMenuSelection = static_cast<MainMenuItem>((current + 1) % static_cast<int>(MainMenuItem::Count));
    buzzer.navBeep();
    screenDirty = true;
  }

  if (buttons[static_cast<size_t>(ButtonId::A)].justPressed() || buttons[static_cast<size_t>(ButtonId::Start)].justPressed()) {
    openMainMenuSelection();
  }
}

void handleSettingsInput(uint32_t now) {
  if (buttons[static_cast<size_t>(ButtonId::Up)].justPressed()) {
    const int current = static_cast<int>(settingsSelection);
    settingsSelection = static_cast<SettingsItem>((current + static_cast<int>(SettingsItem::Count) - 1) % static_cast<int>(SettingsItem::Count));
    buzzer.navBeep();
    screenDirty = true;
  }

  if (buttons[static_cast<size_t>(ButtonId::Down)].justPressed()) {
    const int current = static_cast<int>(settingsSelection);
    settingsSelection = static_cast<SettingsItem>((current + 1) % static_cast<int>(SettingsItem::Count));
    buzzer.navBeep();
    screenDirty = true;
  }

  if (buttons[static_cast<size_t>(ButtonId::A)].justPressed() || buttons[static_cast<size_t>(ButtonId::Start)].justPressed()) {
    buzzer.confirmBeep();
    switch (settingsSelection) {
      case SettingsItem::ServoTest:
        servoController.startTest(now);
        screenDirty = true;
        break;
      case SettingsItem::BuzzerTest:
        buzzerTestActive = true;
        buzzerTestEndsAtMs = now + 250;
        tone(BUZZER_PIN, 1800);
        screenDirty = true;
        break;
      case SettingsItem::Back:
        setScreen(Screen::MainMenu);
        break;
      case SettingsItem::Count:
        break;
    }
  }

  if (buttons[static_cast<size_t>(ButtonId::B)].justPressed() || buttons[static_cast<size_t>(ButtonId::Select)].justPressed()) {
    buzzer.confirmBeep();
    setScreen(Screen::MainMenu);
  }
}

void handleInputTest(uint32_t now) {
  uint8_t newPressedMask = 0;
  for (uint8_t index = 0; index < static_cast<uint8_t>(ButtonId::Count); ++index) {
    if (buttons[index].pressed()) {
      newPressedMask |= (1U << index);
    }
    if (buttons[index].justPressed()) {
      lastPressedButton = index;
      screenDirty = true;
    }
  }

  if (newPressedMask != currentPressedMask) {
    currentPressedMask = newPressedMask;
    screenDirty = true;
  }

  if (buttons[static_cast<size_t>(ButtonId::B)].justPressed() || buttons[static_cast<size_t>(ButtonId::Select)].justPressed()) {
    buzzer.confirmBeep();
    setScreen(Screen::MainMenu);
  }

  (void)now;
}

void updateButtons(uint32_t now) {
  for (Button &button : buttons) {
    button.update(now);
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 init failed");
    for (;;) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.display();

  buzzer.begin();
  servoController.begin();

  for (size_t index = 0; index < buttons.size(); ++index) {
    buttons[index].begin(buttonPins[index]);
  }

  renderScreen();
}

void loop() {
  const uint32_t now = millis();

  updateButtons(now);
  servoController.update(now);

  if (buzzerTestActive && static_cast<int32_t>(now - buzzerTestEndsAtMs) >= 0) {
    noTone(BUZZER_PIN);
    buzzerTestActive = false;
    screenDirty = true;
  }

  switch (currentScreen) {
    case Screen::MainMenu:
      handleMainMenuInput(now);
      break;
    case Screen::InputTest:
      handleInputTest(now);
      break;
    case Screen::Settings:
      handleSettingsInput(now);
      break;
    case Screen::Placeholder:
      if (buttons[static_cast<size_t>(ButtonId::B)].justPressed() || buttons[static_cast<size_t>(ButtonId::Select)].justPressed()) {
        setScreen(Screen::MainMenu);
      }
      break;
  }

  if (screenDirty) {
    renderScreen();
  }
}
