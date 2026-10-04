#include "effects/matrixRainEffect.h"
#include "assets/fonts/matrixCodeFont.h"

#include <new>
#include <string.h>

extern uint16_t globalBrightnessScaleFixed;

static constexpr int kGlyphW = MATRIX_GLYPH_STEP_W;
static constexpr int kGlyphH = MATRIX_GLYPH_STEP_H;

static DisplayOutputType *g_display = nullptr;
static int g_w = 0;
static int g_h = 0;
static int g_numCols = 0;

struct RainColumn {
  int16_t y;
  uint8_t speed;
  uint8_t length;
  uint8_t tick;
  uint8_t seed;
  uint8_t active;
};

static RainColumn *g_cols = nullptr;

static constexpr int kMaxW = 128;
static constexpr int kMaxH = 32;
static uint16_t g_rainMap[kMaxW * kMaxH];
static bool g_rainMapValid = false;

static uint16_t matrixColor(uint8_t level) {
  const uint16_t s = globalBrightnessScaleFixed;
  uint8_t r, g, b;
  if (level >= 240) {
    r = 190;
    g = 255;
    b = 190;
  } else if (level >= 200) {
    r = 20;
    g = 255;
    b = 40;
  } else if (level >= 80) {
    r = 0;
    g = level;
    b = 0;
  } else {
    r = 0;
    g = (uint8_t)((level * 2) / 3);
    b = 0;
  }
  r = (uint16_t(r) * s + 128u) >> 8;
  g = (uint16_t(g) * s + 128u) >> 8;
  b = (uint16_t(b) * s + 128u) >> 8;
  return g_display->color565(r, g, b);
}

static int pickGlyph(uint8_t seed, int index, bool isHead) {
  uint8_t n = (uint8_t)(seed * 31u + (uint8_t)index * 17u);
  if (isHead) {
    n ^= (uint8_t)(millis() >> 3);
  } else {
    n ^= (uint8_t)(millis() >> 7);
  }
  if (MATRIX_CODE_GLYPH_COUNT <= 0) {
    return 0;
  }
  return n % MATRIX_CODE_GLYPH_COUNT;
}

static void drawGlyphAt(int x, int y, int index, uint16_t color) {
  MatrixGlyph g;
  matrixCodeLoadGlyph(index, &g);
  for (int row = 0; row < MATRIX_GLYPH_PX_H; ++row) {
    const uint8_t bits = g.rows[row];
    for (int col = 0; col < MATRIX_GLYPH_PX_W; ++col) {
      if (bits & (1 << (MATRIX_GLYPH_PX_W - 1 - col))) {
        const int px = x + col;
        const int py = y + row;
        if ((unsigned)px < (unsigned)g_w && (unsigned)py < (unsigned)g_h) {
          g_display->drawPixel(px, py, color);
        }
      }
    }
  }
}

static void rebuildRainMap() {
  g_rainMapValid = false;
  if (!g_display || !g_cols || g_w <= 0 || g_h <= 0) {
    return;
  }
  if (g_w > kMaxW || g_h > kMaxH) {
    return;
  }

  memset(g_rainMap, 0, (size_t)g_w * (size_t)g_h * sizeof(uint16_t));

  for (int col = 0; col < g_numCols; ++col) {
    RainColumn &c = g_cols[col];
    if (!c.active || c.length == 0) {
      continue;
    }
    const int px = col * kGlyphW;
    for (int i = 0; i < c.length; ++i) {
      const int py = c.y - i * kGlyphH;
      if (py < -MATRIX_GLYPH_PX_H || py >= g_h) {
        continue;
      }
      const bool isHead = (i == 0);
      const int gi = pickGlyph(c.seed, i, isHead);
      uint8_t level;
      if (isHead) {
        level = 255;
      } else if (i == 1) {
        level = 220;
      } else {
        level = (uint8_t)((180 * (c.length - i)) / c.length);
      }
      const uint16_t color = matrixColor(level);

      MatrixGlyph g;
      matrixCodeLoadGlyph(gi, &g);
      for (int row = 0; row < MATRIX_GLYPH_PX_H; ++row) {
        const int gy = py + row;
        if ((unsigned)gy >= (unsigned)g_h) {
          continue;
        }
        const uint8_t bits = g.rows[row];
        for (int gc = 0; gc < MATRIX_GLYPH_PX_W; ++gc) {
          if (!(bits & (1 << (MATRIX_GLYPH_PX_W - 1 - gc)))) {
            continue;
          }
          const int gx = px + gc;
          if ((unsigned)gx >= (unsigned)g_w) {
            continue;
          }
          g_rainMap[gy * g_w + gx] = color;
        }
      }
    }
  }
  g_rainMapValid = true;
}

static void advanceColumnsOnly() {
  if (!g_cols || g_numCols <= 0) {
    return;
  }
  for (int col = 0; col < g_numCols; ++col) {
    RainColumn &c = g_cols[col];
    if (!c.active) {
      if (random(160) == 0) {
        c.active = 1;
        c.y = (int16_t)random(-g_h / 2, 0);
        c.speed = (uint8_t)(1 + random(3));
        c.length = (uint8_t)(4 + random((g_h / kGlyphH) + 2));
        c.tick = 0;
        c.seed = (uint8_t)random(256);
      }
      continue;
    }
    if (++c.tick >= (4 - c.speed)) {
      c.tick = 0;
      c.y = (int16_t)(c.y + kGlyphH);
      if (random(10) == 0) {
        c.seed ^= (uint8_t)random(256);
      }
    }
    if (c.y - c.length * kGlyphH > g_h) {
      if (random(100) < 28) {
        c.active = 0;
        c.length = 0;
      } else {
        c.y = (int16_t)random(-g_h / 2, 0);
        c.speed = (uint8_t)(1 + random(3));
        c.length = (uint8_t)(4 + random((g_h / kGlyphH) + 2));
        c.seed = (uint8_t)random(256);
      }
    }
  }
}

void initMatrixRainEffect(DisplayOutputType *display) {
  g_display = display;
  if (!g_display) {
    return;
  }
  g_w = (int)g_display->width();
  g_h = (int)g_display->height();
  if (g_w <= 0 || g_h <= 0) {
    return;
  }
  g_numCols = g_w / kGlyphW;
  if (g_numCols < 1) {
    g_numCols = 1;
  }

  delete[] g_cols;
  g_cols = new (std::nothrow) RainColumn[g_numCols];
  if (!g_cols) {
    Serial.println("MatrixRain: alloc failed");
    return;
  }

  for (int i = 0; i < g_numCols; ++i) {
    if (random(100) >= 70) {
      g_cols[i].active = 0;
      g_cols[i].length = 0;
      continue;
    }
    g_cols[i].active = 1;
    g_cols[i].y = (int16_t)random(-g_h, 0);
    g_cols[i].speed = (uint8_t)(1 + random(3));
    g_cols[i].length = (uint8_t)(4 + random((g_h / kGlyphH) + 2));
    g_cols[i].tick = 0;
    g_cols[i].seed = (uint8_t)random(256);
  }

  Serial.printf("MatrixRain init %dx%d cols=%d glyphs=%d\n", g_w, g_h,
                g_numCols, MATRIX_CODE_GLYPH_COUNT);
}

// ---------- Face 1: full-panel rain ----------
void updateAndDrawMatrixRainEffect() {
  if (!g_display || !g_cols || g_numCols <= 0) {
    return;
  }

  advanceColumnsOnly();
  g_display->fillScreen(0);

  for (int col = 0; col < g_numCols; ++col) {
    RainColumn &c = g_cols[col];
    if (!c.active || c.length == 0) {
      continue;
    }
    const int px = col * kGlyphW;
    for (int i = 0; i < c.length; ++i) {
      const int py = c.y - i * kGlyphH;
      if (py < -MATRIX_GLYPH_PX_H || py >= g_h) {
        continue;
      }
      const bool isHead = (i == 0);
      const int gi = pickGlyph(c.seed, i, isHead);
      uint8_t level;
      if (isHead) {
        level = 255;
      } else if (i == 1) {
        level = 220;
      } else {
        level = (uint8_t)((180 * (c.length - i)) / c.length);
      }
      drawGlyphAt(px, py, gi, matrixColor(level));
    }
  }
}

// ---------- Face 2 helpers: stencil rain ----------
void matrixRainAdvance() {
  advanceColumnsOnly();
  rebuildRainMap();
}

uint16_t matrixRainColorAt(int x, int y) {
  if (!g_rainMapValid || (unsigned)x >= (unsigned)g_w ||
      (unsigned)y >= (unsigned)g_h) {
    return 0;
  }
  return g_rainMap[y * g_w + x];
}

void drawMatrixRainThroughXbm(int x, int y, int width, int height,
                              const uint8_t *xbm) {
  if (!g_display || !xbm || width <= 0 || height <= 0) {
    return;
  }

  const int byteWidth = (width + 7) >> 3;

  for (int j = 0; j < height; ++j) {
    const int py = y + j;
    if ((unsigned)py >= (unsigned)g_h) {
      continue;
    }
    const uint8_t *rowPtr = xbm + j * byteWidth;
    for (int i = 0; i < width; ++i) {
      // Adafruit / common XBM: LSB first in each byte
      const uint8_t b = pgm_read_byte(&rowPtr[i >> 3]);
      if (!(b & (1 << (i & 7)))) {
        continue;
      }
      const int px = x + i;
      if ((unsigned)px >= (unsigned)g_w) {
        continue;
      }
      const uint16_t c = matrixRainColorAt(px, py);
      if (c) {
        g_display->drawPixel(px, py, c);
      }
    }
  }
}