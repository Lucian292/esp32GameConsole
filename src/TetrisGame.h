#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include <array>
#include <cstdint>

struct TetrisInput {
  bool upEdge = false;
  bool downEdge = false;
  bool leftEdge = false;
  bool rightEdge = false;
  bool aEdge = false;
  bool bEdge = false;
  bool startEdge = false;
  bool selectEdge = false;
};

class TetrisGame {
 public:
  struct Callbacks {
    void (*onLineClear)(uint8_t linesCleared, uint32_t score) = nullptr;
    void (*onGameOver)() = nullptr;
    void (*onUnlock)(uint32_t score) = nullptr;
  };

  void setCallbacks(Callbacks callbacks);
  void start(uint32_t now);
  void update(uint32_t now, const TetrisInput &input);
  void render(Adafruit_SSD1306 &display) const;

  bool needsRedraw() const;
  void clearRedraw();
  bool shouldReturnToMenu() const;
  void clearReturnToMenuRequest();

 private:
  static constexpr uint8_t BOARD_WIDTH = 10;
  static constexpr uint8_t BOARD_HEIGHT = 14;
  static constexpr uint8_t CELL_SIZE = 4;
  static constexpr uint8_t BOARD_X = 0;
  static constexpr uint8_t BOARD_Y = 8;
  static constexpr uint32_t UNLOCK_SCORE = 100;

  enum class State : uint8_t {
    Idle,
    Playing,
    GameOver,
  };

  struct Piece {
    uint8_t type = 0;
    uint8_t rotation = 0;
    int8_t x = 0;
    int8_t y = 0;
  };

  void reset(uint32_t now);
  void spawnPiece(uint32_t now);
  bool canPlace(uint8_t type, uint8_t rotation, int8_t x, int8_t y) const;
  void lockPiece(uint32_t now);
  uint8_t clearFullLines();
  void applyScore(uint8_t linesCleared, uint32_t now);
  void triggerUnlock(uint32_t score);
  void gameOver();
  void stepFall(uint32_t now);
  void movePiece(int8_t dx, int8_t dy);
  void rotatePiece(int8_t direction);
  void hardDrop(uint32_t now);
  uint32_t fallInterval() const;
  uint32_t softDropInterval() const;
  void setGameRedraw();

  Callbacks callbacks_;
  State state_ = State::Idle;
  std::array<std::array<uint8_t, BOARD_WIDTH>, BOARD_HEIGHT> board_{};
  Piece currentPiece_{};
  bool redraw_ = true;
  bool returnToMenu_ = false;
  bool unlockTriggered_ = false;
  uint32_t score_ = 0;
  uint32_t nextFallAtMs_ = 0;
  uint32_t unlockMessageUntilMs_ = 0;
  bool softDropActive_ = false;
};
