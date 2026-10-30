#include "effects/matrixRainEffect.h"
#include "assets/fonts/matrixCodeFont.h"

#include <new>
#include <string.h>

extern uint16_t globalBrightnessScaleFixed;

static constexpr int kGlyphW = 2;
static constexpr int kGlyphH = 3;
static constexpr int kDrawW = 2;
static constexpr int kDrawH = 3;

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
static bool g_columnsAdvancedThisFrame = false;
static unsigned long g_frameTimeMs = 0;

static uint16_t matrixColor(uint8_t level, uint16_t s = 256) {
  if (!g_display) {
    return 0;
  }
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
    n ^= (uint8_t)(g_frameTimeMs >> 3);
  } else {
    n ^= (uint8_t)(g_frameTimeMs >> 7);
  }
  if (MATRIX_CODE_GLYPH_COUNT <= 0) {
    return 0;
  }
  return n % MATRIX_CODE_GLYPH_COUNT;
}

static void stampGlyph(int x, int y, int index, uint16_t color, bool toMap) {
  MatrixGlyph g;
  matrixCodeLoadGlyph(index, &g);
  for (int row = 0; row < kDrawH; ++row) {
    const int srcRow = (row * MATRIX_GLYPH_PX_H) / kDrawH;
    const uint8_t bits = g.rows[srcRow];
    for (int col = 0; col < kDrawW; ++col) {
      const int srcCol = (col * MATRIX_GLYPH_PX_W) / kDrawW;
      if (!(bits & (1 << (MATRIX_GLYPH_PX_W - 1 - srcCol)))) {
        continue;
      }
      const int px = x + col;
      const int py = y + row;
      if ((unsigned)px >= (unsigned)g_w || (unsigned)py >= (unsigned)g_h) {
        continue;
      }
      if (toMap) {
        g_rainMap[py * g_w + px] = color;
      } else {
        g_display->drawPixel(px, py, color);
      }
    }
  }
}

static uint8_t trailLevel(int i, int length) {
  if (i == 0) {
    return 255;
  }
  if (i == 1) {
    return 220;
  }
  return (uint8_t)((180 * (length - i)) / length);
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
      if (py < -kDrawH || py >= g_h) {
        continue;
      }
      stampGlyph(px, py, pickGlyph(c.seed, i, i == 0),
                 matrixColor(trailLevel(i, c.length)), true);
    }
  }
  g_rainMapValid = true;
}

static void spawnColumn(RainColumn &c) {
  c.active = 1;
  c.y = (int16_t)random(-g_h / 3, 0);
  c.speed = (uint8_t)(1 + random(3));
  c.length = (uint8_t)(14 + random((g_h / kGlyphH) + 8));
  c.tick = 0;
  c.seed = (uint8_t)random(256);
}

static void advanceColumnsOnly() {
  if (!g_cols || g_numCols <= 0) {
    return;
  }
  for (int col = 0; col < g_numCols; ++col) {
    RainColumn &c = g_cols[col];
    if (!c.active) {
      spawnColumn(c);
      continue;
    }
    if (++c.tick >= (uint8_t)(8 + (3 - c.speed) * 4)) {
      c.tick = 0;
      c.y = (int16_t)(c.y + 1);
      if (random(18) == 0) {
        c.seed ^= (uint8_t)random(256);
      }
    }
    if (c.y - c.length * kGlyphH > g_h) {
      spawnColumn(c);
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
    g_cols[i].active = 1;
    g_cols[i].y = (int16_t)random(-g_h, g_h);
    g_cols[i].speed = (uint8_t)(1 + random(3));
    g_cols[i].length = (uint8_t)(14 + random((g_h / kGlyphH) + 8));
    g_cols[i].tick = 0;
    g_cols[i].seed = (uint8_t)random(256);
  }

  Serial.printf("MatrixRain init %dx%d cols=%d glyphs=%d\n", g_w, g_h,
                g_numCols, MATRIX_CODE_GLYPH_COUNT);
}

void updateAndDrawMatrixRainEffect() {
  if (!g_display || !g_cols || g_numCols <= 0) {
    return;
  }

  matrixRainAdvanceColumns();
  g_display->fillScreen(0);

  for (int col = 0; col < g_numCols; ++col) {
    RainColumn &c = g_cols[col];
    if (!c.active || c.length == 0) {
      continue;
    }
    const int px = col * kGlyphW;
    for (int i = 0; i < c.length; ++i) {
      const int py = c.y - i * kGlyphH;
      if (py < -kDrawH || py >= g_h) {
        continue;
      }
      stampGlyph(px, py, pickGlyph(c.seed, i, i == 0),
                 matrixColor(trailLevel(i, c.length), globalBrightnessScaleFixed), false);
    }
  }
}

void matrixRainBeginFrame() {
  g_columnsAdvancedThisFrame = false;
  g_frameTimeMs = millis();
}

void matrixRainAdvance() {
  matrixRainAdvanceColumns();
  if (!g_rainMapValid) rebuildRainMap();
}

// Every shape in a frame samples the same column positions, even if drawing
// crosses a millisecond boundary or the task is preempted between features.
void matrixRainAdvanceColumns() {
  if (g_columnsAdvancedThisFrame) return;
  g_columnsAdvancedThisFrame = true;
  advanceColumnsOnly();
  g_rainMapValid = false;
}

static uint16_t matrixRainRawColorAt(int x, int y) {
  if ((unsigned)x >= (unsigned)g_w || (unsigned)y >= (unsigned)g_h) {
    return 0;
  }
  if (g_rainMapValid) {
    const uint16_t exact = g_rainMap[y * g_w + x];
    if (exact) {
      return exact;
    }
  }
  if (!g_cols || g_numCols <= 0 || kGlyphW <= 0) {
    return 0;
  }
  int col = x / kGlyphW;
  if (col >= g_numCols) {
    col = g_numCols - 1;
  }
  RainColumn &c = g_cols[col];
  if (!c.active || c.length == 0) {
    return 0;
  }
  const int head = c.y;
  const int tail = c.y - c.length * kGlyphH;
  if (y > head + 1 || y < tail) {
    return 0;
  }
  const int dist = head - y;
  const int span = head - tail;
  uint8_t level = 40;
  if (dist <= 1) {
    level = 255;
  } else if (span > 0) {
    level = (uint8_t)(40 + (180 * (span - dist)) / span);
  }
  return matrixColor(level);
}

// Keep the cached glyph map unscaled so callers can apply their own face/mic curve.
uint16_t matrixRainColorAt(int x, int y, uint16_t brightnessScale) {
  const uint16_t c = matrixRainRawColorAt(x, y);
  const uint16_t s = brightnessScale == UINT16_MAX ? globalBrightnessScaleFixed : brightnessScale;
  if (s == 256) return c;
  const uint8_t r = static_cast<uint8_t>((((c >> 11) & 31) * 255u / 31 * s + 128) >> 8);
  const uint8_t g = static_cast<uint8_t>((((c >> 5) & 63) * 255u / 63 * s + 128) >> 8);
  const uint8_t b = static_cast<uint8_t>(((c & 31) * 255u / 31 * s + 128) >> 8);
  return g_display ? g_display->color565(r, g, b) : 0;
}

void drawMatrixRainThroughXbm(int x, int y, int width, int height,
                              const uint8_t *xbm, uint16_t brightnessScale) {
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
      const uint8_t b = pgm_read_byte(&rowPtr[i >> 3]);
      if (!(b & (uint8_t)(0x80U >> (i & 7)))) {
        continue;
      }
      const int px = x + i;
      if ((unsigned)px >= (unsigned)g_w) {
        continue;
      }
      const uint16_t c = matrixRainColorAt(px, py, brightnessScale);
      if (c) {
        g_display->drawPixel(px, py, c);
      }
    }
  }
}