#include "SnakeGame.h"

// Seteaza functiile callback apelate de joc pentru sunete si unlock.
void SnakeGame::setCallbacks(Callbacks callbacks) {
  callbacks_ = callbacks;
}

// Porneste jocul prin resetarea completa a starii.
void SnakeGame::start(uint32_t now) {
  reset(now);
}

// Initializeaza o runda noua de Snake.
void SnakeGame::reset(uint32_t now) {
  state_ = State::Playing;
  direction_ = Direction::Right;
  pendingDirection_ = Direction::Right;
  length_ = 3;
  score_ = 0;
  redraw_ = true;
  returnToMenu_ = false;
  unlockTriggered_ = false;
  unlockMessageUntilMs_ = 0;
  nextStepAtMs_ = now + stepInterval();

  const uint8_t startX = GRID_WIDTH / 2;
  const uint8_t startY = GRID_HEIGHT / 2;

  for (uint8_t index = 0; index < length_; ++index) {
    snake_[index].x = static_cast<uint8_t>(startX - index);
    snake_[index].y = startY;
  }

  placeFood();
}

// Update principal: proceseaza input, timing si stari speciale.
void SnakeGame::update(uint32_t now, const SnakeInput &input) {
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

  if (input.upEdge && !isOpposite(Direction::Up, direction_)) {
    pendingDirection_ = Direction::Up;
  } else if (input.rightEdge && !isOpposite(Direction::Right, direction_)) {
    pendingDirection_ = Direction::Right;
  } else if (input.downEdge && !isOpposite(Direction::Down, direction_)) {
    pendingDirection_ = Direction::Down;
  } else if (input.leftEdge && !isOpposite(Direction::Left, direction_)) {
    pendingDirection_ = Direction::Left;
  }

  if (input.bEdge || input.selectEdge) {
    returnToMenu_ = true;
    return;
  }

  if (static_cast<int32_t>(now - nextStepAtMs_) < 0) {
    return;
  }

  step(now);
}

// Randare completa a HUD-ului si a tablei de joc.
void SnakeGame::render(Adafruit_SSD1306 &display) const {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Snake");
  display.setCursor(68, 0);
  display.print("Score:");
  display.print(score_);

  if (unlockTriggered_) {
    display.setCursor(96, 0);
    display.print("U");
  }

  display.drawFastHLine(0, HUD_HEIGHT - 1, 128, SSD1306_WHITE);

  if (state_ == State::Playing) {
    for (uint8_t index = 0; index < length_; ++index) {
      const uint8_t x = snake_[index].x;
      const uint8_t y = snake_[index].y;
      const int16_t px = x * CELL_SIZE;
      const int16_t py = HUD_HEIGHT + y * CELL_SIZE;
      if (index == 0) {
        display.fillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, SSD1306_WHITE);
      } else {
        display.drawRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, SSD1306_WHITE);
      }
    }

    display.fillRect(food_.x * CELL_SIZE + 2, HUD_HEIGHT + food_.y * CELL_SIZE + 2, CELL_SIZE - 4, CELL_SIZE - 4, SSD1306_WHITE);

    if (unlockTriggered_ && static_cast<int32_t>(millis() - unlockMessageUntilMs_) < 0) {
      display.setCursor(78, 8);
      display.print("UNLOCKED");
    }
  } else if (state_ == State::GameOver) {
    display.setCursor(20, 18);
    display.print("GAME OVER");
    display.setCursor(8, 32);
    display.print("Score:");
    display.print(score_);
    display.setCursor(8, 44);
    display.print("A/START retry");
    display.setCursor(8, 54);
    display.print("B/SELECT menu");
  }

  display.display();
}

// Indica daca jocul cere rerandare.
bool SnakeGame::needsRedraw() const {
  return redraw_;
}

// Marcheaza rerandarea ca efectuata.
void SnakeGame::clearRedraw() {
  redraw_ = false;
}

// Indica daca jocul cere revenire in meniu.
bool SnakeGame::shouldReturnToMenu() const {
  return returnToMenu_;
}

// Curata cererea de revenire in meniu dupa ce a fost tratata.
void SnakeGame::clearReturnToMenuRequest() {
  returnToMenu_ = false;
}

// Genereaza o pozitie aleatoare pentru hrana, in afara corpului sarpelui.
void SnakeGame::placeFood() {
  do {
    food_.x = static_cast<uint8_t>(random(GRID_WIDTH));
    food_.y = static_cast<uint8_t>(random(GRID_HEIGHT));
  } while (containsPoint(food_.x, food_.y));
}

// Verifica daca un punct este ocupat de corpul sarpelui.
bool SnakeGame::containsPoint(uint8_t x, uint8_t y) const {
  for (uint8_t index = 0; index < length_; ++index) {
    if (snake_[index].x == x && snake_[index].y == y) {
      return true;
    }
  }
  return false;
}

// Verifica daca doua directii sunt opuse (interzis pentru schimbare instant).
bool SnakeGame::isOpposite(Direction a, Direction b) const {
  return (a == Direction::Up && b == Direction::Down) ||
         (a == Direction::Down && b == Direction::Up) ||
         (a == Direction::Left && b == Direction::Right) ||
         (a == Direction::Right && b == Direction::Left);
}

// Ajusteaza viteza jocului in functie de scor.
uint32_t SnakeGame::stepInterval() const {
  uint32_t interval = 380;
  if (score_ > 8) {
    interval = 240;
  } else if (score_ > 4) {
    interval = 300;
  } else if (score_ > 1) {
    interval = 340;
  }
  return interval;
}

// Declanseaza deblocarea servo-ului si afisarea mesajului de unlock.
void SnakeGame::triggerUnlock(uint32_t now) {
  unlockTriggered_ = true;
  unlockMessageUntilMs_ = now + 1200;
  if (callbacks_.onUnlock != nullptr) {
    callbacks_.onUnlock(now);
  }
}

// Trecere in starea GameOver si notificare prin callback.
void SnakeGame::gameOver() {
  state_ = State::GameOver;
  redraw_ = true;
  if (callbacks_.onGameOver != nullptr) {
    callbacks_.onGameOver();
  }
}

// Executa un pas de joc: miscare, coliziuni, hrana, scor si unlock.
void SnakeGame::step(uint32_t now) {
  direction_ = pendingDirection_;

  Point head = snake_[0];
  switch (direction_) {
    case Direction::Up:
      if (head.y == 0) {
        gameOver();
        return;
      }
      --head.y;
      break;
    case Direction::Down:
      if (head.y + 1 >= GRID_HEIGHT) {
        gameOver();
        return;
      }
      ++head.y;
      break;
    case Direction::Left:
      if (head.x == 0) {
        gameOver();
        return;
      }
      --head.x;
      break;
    case Direction::Right:
      if (head.x + 1 >= GRID_WIDTH) {
        gameOver();
        return;
      }
      ++head.x;
      break;
  }

  for (uint8_t index = length_; index > 0; --index) {
    snake_[index] = snake_[index - 1];
  }
  snake_[0] = head;

  // Ramane pentru claritate: capul nou nu este tratat ca auto-coliziune.
  if (containsPoint(head.x, head.y) && !(snake_[1].x == head.x && snake_[1].y == head.y)) {
    // Verificarea reala de auto-coliziune se face in bucla de mai jos.
  }

  for (uint8_t index = 1; index < length_; ++index) {
    if (snake_[index].x == head.x && snake_[index].y == head.y) {
      gameOver();
      return;
    }
  }

  if (head.x == food_.x && head.y == food_.y) {
    if (length_ < MAX_SNAKE_LENGTH) {
      ++length_;
    }
    ++score_;
    if (callbacks_.onFood != nullptr) {
      callbacks_.onFood();
    }
    if (!unlockTriggered_ && score_ >= UNLOCK_SCORE) {
      triggerUnlock(now);
    }
    placeFood();
  }

  // Programeaza urmatorul pas in functie de viteza curenta.
  nextStepAtMs_ = now + stepInterval();
  redraw_ = true;
}