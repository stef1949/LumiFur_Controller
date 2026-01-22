#include "dino_game.h"

#include <Arduino.h>
#include <Fonts/TomThumb.h>
#include <cstdio>

#include "bitmaps.h"

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

extern MatrixPanel_I2S_DMA *dma_display;
extern void drawXbm565(int x, int y, int width, int height, const uint8_t *xbm, uint16_t color);

namespace
{
constexpr int DINO_SPRITE_WIDTH = 15;
constexpr int DINO_SPRITE_HEIGHT = 16;
constexpr int DINO_CACTUS_WIDTH = 5;
constexpr int DINO_CACTUS_HEIGHT = 6;
constexpr int DINO_GROUND_THICKNESS = 1;
constexpr int DINO_OBSTACLE_COUNT = 3;
constexpr int DINO_GAME_DINO_X = 8;
constexpr int DINO_HIT_INSET = 2;
constexpr int DINO_CACTUS_HIT_INSET = 1;
constexpr float DINO_GRAVITY = 200.0f;
constexpr float DINO_JUMP_VELOCITY = -100.0f;
constexpr float DINO_BASE_SPEED = 40.0f;
constexpr float DINO_SPEED_BONUS_PER_PIXEL = 0.01f;
constexpr float DINO_MAX_SPEED_BONUS = 25.0f;
constexpr uint32_t DINO_MAX_FRAME_STEP_MS = 60;

struct DinoGameState
{
  bool initialized = false;
  bool gameOver = false;
  volatile bool jumpQueued = false;
  float dinoY = 0.0f;
  float dinoVelY = 0.0f;
  float distance = 0.0f;
  uint32_t lastUpdateMs = 0;
  uint32_t rngState = 0;
  float obstaclesX[DINO_OBSTACLE_COUNT] = {};
};

static DinoGameState gDinoGame;

static void getDinoGapRange(int displayWidth, int &minGap, int &maxGap)
{
  minGap = displayWidth / 3;
  if (minGap < 28)
  {
    minGap = 28;
  }
  maxGap = displayWidth / 2;
  if (maxGap < minGap + 12)
  {
    maxGap = minGap + 12;
  }
}

static uint32_t nextDinoRand()
{
  // LCG constants for a simple, fast RNG local to dino view.
  gDinoGame.rngState = gDinoGame.rngState * 1664525u + 1013904223u;
  if (gDinoGame.rngState == 0)
  {
    gDinoGame.rngState = 0x6d2b79f5u;
  }
  return gDinoGame.rngState;
}

static int dinoRandRange(int minVal, int maxVal)
{
  if (maxVal <= minVal)
  {
    return minVal;
  }
  const uint32_t span = static_cast<uint32_t>(maxVal - minVal);
  return minVal + static_cast<int>(nextDinoRand() % span);
}

static void updateDinoGame(uint32_t now)
{
  if (!dma_display)
  {
    gDinoGame.initialized = false;
    return;
  }

  if (!gDinoGame.initialized)
  {
    resetDinoGame(now);
    return;
  }

  const int displayWidth = dma_display->width();
  const int displayHeight = dma_display->height();
  const int groundY = displayHeight - DINO_GROUND_THICKNESS;
  const float groundTop = static_cast<float>(groundY - DINO_SPRITE_HEIGHT);

  if (gDinoGame.jumpQueued)
  {
    gDinoGame.jumpQueued = false;
    if (gDinoGame.gameOver)
    {
      resetDinoGame(now);
      return;
    }
    if (gDinoGame.dinoY >= groundTop - 0.5f)
    {
      gDinoGame.dinoVelY = DINO_JUMP_VELOCITY;
    }
  }

  uint32_t deltaMs = now - gDinoGame.lastUpdateMs;
  if (deltaMs > DINO_MAX_FRAME_STEP_MS)
  {
    deltaMs = DINO_MAX_FRAME_STEP_MS;
  }
  gDinoGame.lastUpdateMs = now;

  const float dt = static_cast<float>(deltaMs) * 0.001f;

  if (gDinoGame.gameOver)
  {
    if (gDinoGame.dinoY < groundTop)
    {
      gDinoGame.dinoVelY += DINO_GRAVITY * dt;
      gDinoGame.dinoY += gDinoGame.dinoVelY * dt;
      if (gDinoGame.dinoY > groundTop)
      {
        gDinoGame.dinoY = groundTop;
        gDinoGame.dinoVelY = 0.0f;
      }
    }
    return;
  }

  float speedBonus = gDinoGame.distance * DINO_SPEED_BONUS_PER_PIXEL;
  if (speedBonus > DINO_MAX_SPEED_BONUS)
  {
    speedBonus = DINO_MAX_SPEED_BONUS;
  }
  const float speed = DINO_BASE_SPEED + speedBonus;

  gDinoGame.distance += speed * dt;

  gDinoGame.dinoVelY += DINO_GRAVITY * dt;
  gDinoGame.dinoY += gDinoGame.dinoVelY * dt;
  if (gDinoGame.dinoY >= groundTop)
  {
    gDinoGame.dinoY = groundTop;
    gDinoGame.dinoVelY = 0.0f;
  }

  int minGap = 0;
  int maxGap = 0;
  getDinoGapRange(displayWidth, minGap, maxGap);

  float farthestX = static_cast<float>(displayWidth);
  for (int i = 0; i < DINO_OBSTACLE_COUNT; ++i)
  {
    if (gDinoGame.obstaclesX[i] > farthestX)
    {
      farthestX = gDinoGame.obstaclesX[i];
    }
  }

  for (int i = 0; i < DINO_OBSTACLE_COUNT; ++i)
  {
    gDinoGame.obstaclesX[i] -= speed * dt;
    if (gDinoGame.obstaclesX[i] + DINO_CACTUS_WIDTH < 0.0f)
    {
      float gap = static_cast<float>(dinoRandRange(minGap, maxGap));
      gDinoGame.obstaclesX[i] = farthestX + gap;
      farthestX = gDinoGame.obstaclesX[i];
    }
  }

  const int dinoX = DINO_GAME_DINO_X;
  const int dinoY = static_cast<int>(gDinoGame.dinoY + 0.5f);
  const int dinoHitX = dinoX + DINO_HIT_INSET;
  const int dinoHitY = dinoY + DINO_HIT_INSET;
  const int dinoHitW = DINO_SPRITE_WIDTH - (DINO_HIT_INSET * 2);
  const int dinoHitH = DINO_SPRITE_HEIGHT - (DINO_HIT_INSET * 2);

  const int cactusY = groundY - DINO_CACTUS_HEIGHT;
  const int cactusHitY = cactusY + DINO_CACTUS_HIT_INSET;
  const int cactusHitH = DINO_CACTUS_HEIGHT - (DINO_CACTUS_HIT_INSET * 2);

  for (int i = 0; i < DINO_OBSTACLE_COUNT; ++i)
  {
    const int cactusX = static_cast<int>(gDinoGame.obstaclesX[i] + 0.5f);
    const int cactusHitX = cactusX + DINO_CACTUS_HIT_INSET;
    const int cactusHitW = DINO_CACTUS_WIDTH - (DINO_CACTUS_HIT_INSET * 2);

    if (dinoHitX < cactusHitX + cactusHitW &&
        dinoHitX + dinoHitW > cactusHitX &&
        dinoHitY < cactusHitY + cactusHitH &&
        dinoHitY + dinoHitH > cactusHitY)
    {
      gDinoGame.gameOver = true;
      gDinoGame.dinoVelY = 0.0f;
      break;
    }
  }
}
} // namespace

void resetDinoGame(uint32_t now)
{
  if (!dma_display)
  {
    gDinoGame.initialized = false;
    return;
  }

  const int displayWidth = dma_display->width();
  const int displayHeight = dma_display->height();
  const int groundY = displayHeight - DINO_GROUND_THICKNESS;

  gDinoGame.initialized = true;
  gDinoGame.gameOver = false;
  gDinoGame.jumpQueued = false;
  gDinoGame.dinoVelY = 0.0f;
  gDinoGame.dinoY = static_cast<float>(groundY - DINO_SPRITE_HEIGHT);
  gDinoGame.distance = 0.0f;
  gDinoGame.lastUpdateMs = now;
  gDinoGame.rngState = (static_cast<uint32_t>(micros()) << 16) ^ now;
  if (gDinoGame.rngState == 0)
  {
    gDinoGame.rngState = 0x6d2b79f5u;
  }

  int minGap = 0;
  int maxGap = 0;
  getDinoGapRange(displayWidth, minGap, maxGap);

  float spawnX = static_cast<float>(displayWidth + 10);
  for (int i = 0; i < DINO_OBSTACLE_COUNT; ++i)
  {
    spawnX += static_cast<float>(dinoRandRange(minGap, maxGap));
    gDinoGame.obstaclesX[i] = spawnX;
  }
}

void queueDinoJump()
{
  gDinoGame.jumpQueued = true;
}

void renderDinoGameView()
{
  if (!dma_display)
  {
    return;
  }

  const uint32_t now = millis();
  updateDinoGame(now);

  const int displayWidth = dma_display->width();
  const int displayHeight = dma_display->height();
  const int groundY = displayHeight - DINO_GROUND_THICKNESS;
  const int dinoX = DINO_GAME_DINO_X;
  const int dinoY = static_cast<int>(gDinoGame.dinoY + 0.5f);
  const int cactusY = groundY - DINO_CACTUS_HEIGHT;

  const uint16_t dinoColor = dma_display->color565(255, 255, 255);
  const uint16_t cactusColor = dma_display->color565(0, 200, 0);
  const uint16_t groundColor = dma_display->color565(80, 80, 80);
  const uint16_t scoreColor = dma_display->color565(200, 200, 200);

  dma_display->fillRect(0, groundY, displayWidth, DINO_GROUND_THICKNESS, groundColor);
  drawXbm565(dinoX, dinoY, DINO_SPRITE_WIDTH, DINO_SPRITE_HEIGHT, dino, dinoColor);

  for (int i = 0; i < DINO_OBSTACLE_COUNT; ++i)
  {
    const int cactusX = static_cast<int>(gDinoGame.obstaclesX[i] + 0.5f);
    drawXbm565(cactusX, cactusY, DINO_CACTUS_WIDTH, DINO_CACTUS_HEIGHT, cactus, cactusColor);
  }

  char scoreText[12];
  const uint32_t scoreValue = static_cast<uint32_t>(gDinoGame.distance);
  snprintf(scoreText, sizeof(scoreText), "%lu", static_cast<unsigned long>(scoreValue));
  dma_display->setFont(&TomThumb);
  dma_display->setTextSize(1);
  dma_display->setTextColor(scoreColor);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  dma_display->getTextBounds(scoreText, 0, 0, &x1, &y1, &w, &h);
  int scoreX = displayWidth - static_cast<int>(w) - 2;
  if (scoreX < 0)
  {
    scoreX = 0;
  }
  const int scoreY = static_cast<int>(h) + 1;
  dma_display->setCursor(scoreX, scoreY);
  dma_display->print(scoreText);

  if (gDinoGame.gameOver)
  {
    const char *message = "GAME OVER";
    dma_display->setTextColor(dma_display->color565(255, 80, 80));
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    int16_t msgX1 = 0;
    int16_t msgY1 = 0;
    uint16_t msgW = 0;
    uint16_t msgH = 0;
    dma_display->getTextBounds(message, 0, 0, &msgX1, &msgY1, &msgW, &msgH);
    int textX = (displayWidth - static_cast<int>(msgW)) / 2;
    int textY = (displayHeight / 2) + (static_cast<int>(msgH) / 2);
    if (textY < static_cast<int>(msgH))
    {
      textY = static_cast<int>(msgH);
    }
    dma_display->setCursor(textX - msgX1, textY);
    dma_display->print(message);
    dma_display->setCursor(textX + msgX1, textY);
    dma_display->print(message);
  }
}
