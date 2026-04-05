#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include <array>

struct SnakeInput {
  bool upEdge = false;
  bool downEdge = false;
  bool leftEdge = false;
  bool rightEdge = false;
  bool aEdge = false;
  bool bEdge = false;
  bool startEdge = false;
  bool selectEdge = false;
};

class SnakeGame {
 public:
  struct Callbacks {
    void (*onFood)() = nullptr;
    void (*onGameOver)() = nullptr;
    void (*onUnlock)(uint32_t now) = nullptr;
  };

  void setCallbacks(Callbacks callbacks);
  void start(uint32_t now);
  void update(uint32_t now, const SnakeInput &input);
  void render(Adafruit_SSD1306 &display) const;

  bool needsRedraw() const;
  void clearRedraw();
  bool shouldReturnToMenu() const;
  void clearReturnToMenuRequest();

 private:
  static constexpr uint8_t GRID_WIDTH = 16;
  static constexpr uint8_t GRID_HEIGHT = 7;
  static constexpr uint8_t CELL_SIZE = 8;
  static constexpr uint8_t HUD_HEIGHT = 10;
  static constexpr uint8_t MAX_SNAKE_LENGTH = GRID_WIDTH * GRID_HEIGHT;
  static constexpr uint8_t UNLOCK_SCORE = 3;

  enum class State : uint8_t {
    Idle,
    Playing,
    GameOver,
  };

  enum class Direction : uint8_t {
    Up,
    Right,
    Down,
    Left,
  };

  struct Point {
    uint8_t x = 0;
    uint8_t y = 0;
  };

  void reset(uint32_t now);
  void placeFood();
  bool containsPoint(uint8_t x, uint8_t y) const;
  bool isOpposite(Direction a, Direction b) const;
  void step(uint32_t now);
  void gameOver();
  void triggerUnlock(uint32_t now);
  uint32_t stepInterval() const;

  Callbacks callbacks_;
  State state_ = State::Idle;
  Direction direction_ = Direction::Right;
  Direction pendingDirection_ = Direction::Right;
  std::array<Point, MAX_SNAKE_LENGTH> snake_{};
  uint8_t length_ = 0;
  uint8_t score_ = 0;
  Point food_{};
  bool redraw_ = true;
  bool returnToMenu_ = false;
  bool unlockTriggered_ = false;
  uint32_t nextStepAtMs_ = 0;
  uint32_t unlockMessageUntilMs_ = 0;
};