#include "TetrisGame.h"

namespace {

struct ShapeSet {
  std::array<uint16_t, 4> rotations;
};

constexpr std::array<ShapeSet, 7> kShapes = {{
    ShapeSet{{0x0F00, 0x2222, 0x00F0, 0x4444}},
    ShapeSet{{0x0660, 0x0660, 0x0660, 0x0660}},
    ShapeSet{{0x0E40, 0x4C40, 0x4E00, 0x4640}},
    ShapeSet{{0x0E20, 0x44C0, 0x8E00, 0xC440}},
    ShapeSet{{0x0C60, 0x2640, 0x0C60, 0x2640}},
    ShapeSet{{0x08E0, 0x4C80, 0x0E20, 0x2640}},
    ShapeSet{{0x02E0, 0x44C0, 0x0E80, 0xC440}},
}};

constexpr uint8_t kCellBitPositions[16][2] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0},
    {0, 1}, {1, 1}, {2, 1}, {3, 1},
    {0, 2}, {1, 2}, {2, 2}, {3, 2},
    {0, 3}, {1, 3}, {2, 3}, {3, 3},
};

}  // namespace

void TetrisGame::setCallbacks(Callbacks callbacks) {
  callbacks_ = callbacks;
}

void TetrisGame::start(uint32_t now) {
  reset(now);
}

void TetrisGame::reset(uint32_t now) {
  for (auto &row : board_) {
    row.fill(0);
  }

  state_ = State::Playing;
  score_ = 0;
  redraw_ = true;
  returnToMenu_ = false;
  unlockTriggered_ = false;
  unlockMessageUntilMs_ = 0;
  softDropActive_ = false;
  nextFallAtMs_ = now + fallInterval();
  spawnPiece(now);
}

void TetrisGame::spawnPiece(uint32_t now) {
  currentPiece_.type = static_cast<uint8_t>(random(static_cast<int>(kShapes.size())));
  currentPiece_.rotation = 0;
  currentPiece_.x = 3;
  currentPiece_.y = 0;

  if (!canPlace(currentPiece_.type, currentPiece_.rotation, currentPiece_.x, currentPiece_.y)) {
    gameOver();
    return;
  }

  nextFallAtMs_ = now + fallInterval();
  setGameRedraw();
}

bool TetrisGame::canPlace(uint8_t type, uint8_t rotation, int8_t x, int8_t y) const {
  const uint16_t shape = kShapes[type].rotations[rotation & 3U];
  for (uint8_t index = 0; index < 16; ++index) {
    if ((shape & (1U << (15 - index))) == 0) {
      continue;
    }

    const int8_t cellX = static_cast<int8_t>(x + kCellBitPositions[index][0]);
    const int8_t cellY = static_cast<int8_t>(y + kCellBitPositions[index][1]);

    if (cellX < 0 || cellX >= BOARD_WIDTH || cellY < 0 || cellY >= BOARD_HEIGHT) {
      return false;
    }

    if (board_[cellY][cellX] != 0) {
      return false;
    }
  }

  return true;
}

void TetrisGame::update(uint32_t now, const TetrisInput &input) {
  if (state_ == State::Idle) {
    return;
  }

  if (state_ == State::GameOver) {
    if (input.aEdge || input.startEdge) {
      reset(now);
      return;
    }

    if (input.bEdge || input.selectEdge) {
      returnToMenu_ = true;
    }

    return;
  }

  if (input.bEdge || input.selectEdge) {
    returnToMenu_ = true;
    return;
  }

  softDropActive_ = input.downEdge;

  if (input.leftEdge) {
    movePiece(-1, 0);
  }
  if (input.rightEdge) {
    movePiece(1, 0);
  }
  if (input.upEdge || input.aEdge) {
    rotatePiece(1);
  }
  if (input.startEdge) {
    hardDrop(now);
    return;
  }

  const uint32_t interval = softDropActive_ ? softDropInterval() : fallInterval();
  if (static_cast<int32_t>(now - nextFallAtMs_) >= 0) {
    stepFall(now);
    nextFallAtMs_ = now + interval;
  }
}

void TetrisGame::render(Adafruit_SSD1306 &display) const {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Tetris");
  display.setCursor(56, 0);
  display.print("Score:");
  display.print(score_);

  if (unlockTriggered_) {
    display.setCursor(112, 0);
    display.print("U");
  }

  display.drawFastHLine(0, BOARD_Y - 1, 128, SSD1306_WHITE);
  display.drawRect(BOARD_X, BOARD_Y, BOARD_WIDTH * CELL_SIZE, BOARD_HEIGHT * CELL_SIZE, SSD1306_WHITE);

  for (uint8_t row = 0; row < BOARD_HEIGHT; ++row) {
    for (uint8_t column = 0; column < BOARD_WIDTH; ++column) {
      if (board_[row][column] == 0) {
        continue;
      }
      const int16_t px = BOARD_X + column * CELL_SIZE;
      const int16_t py = BOARD_Y + row * CELL_SIZE;
      display.fillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, SSD1306_WHITE);
    }
  }

  if (state_ == State::Playing) {
    const uint16_t shape = kShapes[currentPiece_.type].rotations[currentPiece_.rotation & 3U];
    for (uint8_t index = 0; index < 16; ++index) {
      if ((shape & (1U << (15 - index))) == 0) {
        continue;
      }

      const int8_t cellX = static_cast<int8_t>(currentPiece_.x + kCellBitPositions[index][0]);
      const int8_t cellY = static_cast<int8_t>(currentPiece_.y + kCellBitPositions[index][1]);
      if (cellX < 0 || cellX >= BOARD_WIDTH || cellY < 0 || cellY >= BOARD_HEIGHT) {
        continue;
      }

      const int16_t px = BOARD_X + cellX * CELL_SIZE;
      const int16_t py = BOARD_Y + cellY * CELL_SIZE;
      display.fillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, SSD1306_WHITE);
    }

    display.setCursor(48, 14);
    display.print("Move: L/R");
    display.setCursor(48, 24);
    display.print("Rotate: A/U");
    display.setCursor(48, 34);
    display.print("Drop: START");
    display.setCursor(48, 44);
    display.print("Back: B/SEL");

    if (unlockTriggered_ && static_cast<int32_t>(millis() - unlockMessageUntilMs_) < 0) {
      display.setCursor(48, 54);
      display.print("UNLOCKED");
    }
  } else if (state_ == State::GameOver) {
    display.setCursor(42, 18);
    display.print("GAME OVER");
    display.setCursor(42, 30);
    display.print("Score:");
    display.print(score_);
    display.setCursor(42, 42);
    display.print("A/START retry");
    display.setCursor(42, 52);
    display.print("B/SELECT menu");
  }

  display.display();
}

bool TetrisGame::needsRedraw() const {
  return redraw_;
}

void TetrisGame::clearRedraw() {
  redraw_ = false;
}

bool TetrisGame::shouldReturnToMenu() const {
  return returnToMenu_;
}

void TetrisGame::clearReturnToMenuRequest() {
  returnToMenu_ = false;
}

void TetrisGame::setGameRedraw() {
  redraw_ = true;
}

void TetrisGame::movePiece(int8_t dx, int8_t dy) {
  if (!canPlace(currentPiece_.type, currentPiece_.rotation, static_cast<int8_t>(currentPiece_.x + dx), static_cast<int8_t>(currentPiece_.y + dy))) {
    return;
  }

  currentPiece_.x = static_cast<int8_t>(currentPiece_.x + dx);
  currentPiece_.y = static_cast<int8_t>(currentPiece_.y + dy);
  setGameRedraw();
}

void TetrisGame::rotatePiece(int8_t direction) {
  const uint8_t nextRotation = static_cast<uint8_t>((currentPiece_.rotation + direction + 4) & 3);
  if (!canPlace(currentPiece_.type, nextRotation, currentPiece_.x, currentPiece_.y)) {
    return;
  }

  currentPiece_.rotation = nextRotation;
  setGameRedraw();
}

void TetrisGame::hardDrop(uint32_t now) {
  while (canPlace(currentPiece_.type, currentPiece_.rotation, currentPiece_.x, static_cast<int8_t>(currentPiece_.y + 1))) {
    ++currentPiece_.y;
  }
  lockPiece(now);
}

void TetrisGame::stepFall(uint32_t now) {
  if (canPlace(currentPiece_.type, currentPiece_.rotation, currentPiece_.x, static_cast<int8_t>(currentPiece_.y + 1))) {
    ++currentPiece_.y;
    setGameRedraw();
    return;
  }

  lockPiece(now);
}

void TetrisGame::lockPiece(uint32_t now) {
  const uint16_t shape = kShapes[currentPiece_.type].rotations[currentPiece_.rotation & 3U];
  for (uint8_t index = 0; index < 16; ++index) {
    if ((shape & (1U << (15 - index))) == 0) {
      continue;
    }

    const int8_t cellX = static_cast<int8_t>(currentPiece_.x + kCellBitPositions[index][0]);
    const int8_t cellY = static_cast<int8_t>(currentPiece_.y + kCellBitPositions[index][1]);
    if (cellX < 0 || cellX >= BOARD_WIDTH || cellY < 0 || cellY >= BOARD_HEIGHT) {
      continue;
    }

    board_[cellY][cellX] = 1;
  }

  const uint8_t clearedLines = clearFullLines();
  applyScore(clearedLines, now);
  spawnPiece(now);
}

uint8_t TetrisGame::clearFullLines() {
  uint8_t clearedLines = 0;

  for (int8_t row = BOARD_HEIGHT - 1; row >= 0; --row) {
    bool full = true;
    for (uint8_t column = 0; column < BOARD_WIDTH; ++column) {
      if (board_[row][column] == 0) {
        full = false;
        break;
      }
    }

    if (!full) {
      continue;
    }

    ++clearedLines;
    for (int8_t moveRow = row; moveRow > 0; --moveRow) {
      board_[moveRow] = board_[moveRow - 1];
    }
    board_[0].fill(0);
    ++row;
  }

  if (clearedLines > 0) {
    setGameRedraw();
  }

  return clearedLines;
}

void TetrisGame::applyScore(uint8_t linesCleared, uint32_t now) {
  if (linesCleared == 0) {
    return;
  }

  score_ += static_cast<uint32_t>(linesCleared) * 100U;
  unlockMessageUntilMs_ = now + 1200;

  if (callbacks_.onLineClear != nullptr) {
    callbacks_.onLineClear(linesCleared, score_);
  }

  if (!unlockTriggered_ && score_ >= UNLOCK_SCORE) {
    triggerUnlock(score_);
  }
}

void TetrisGame::triggerUnlock(uint32_t score) {
  unlockTriggered_ = true;
  if (callbacks_.onUnlock != nullptr) {
    callbacks_.onUnlock(score);
  }
}

void TetrisGame::gameOver() {
  state_ = State::GameOver;
  redraw_ = true;
  if (callbacks_.onGameOver != nullptr) {
    callbacks_.onGameOver();
  }
}

uint32_t TetrisGame::fallInterval() const {
  if (score_ >= 800) {
    return 170;
  }
  if (score_ >= 500) {
    return 220;
  }
  if (score_ >= 300) {
    return 280;
  }
  if (score_ >= 100) {
    return 360;
  }
  return 480;
}

uint32_t TetrisGame::softDropInterval() const {
  return 90;
}
