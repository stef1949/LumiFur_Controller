#ifndef MATRIX_CODE_FONT_H
#define MATRIX_CODE_FONT_H

#include <Arduino.h>

struct MatrixGlyph {
  uint8_t rows[6];
};

static constexpr int MATRIX_GLYPH_PX_W = 4;
static constexpr int MATRIX_GLYPH_PX_H = 6;
static constexpr int MATRIX_GLYPH_STEP_W = 5;
static constexpr int MATRIX_GLYPH_STEP_H = 7;

extern const MatrixGlyph MATRIX_CODE_GLYPHS[] PROGMEM;
extern const int MATRIX_CODE_GLYPH_COUNT;

inline void matrixCodeLoadGlyph(int index, MatrixGlyph *out) {
  if (!out || MATRIX_CODE_GLYPH_COUNT <= 0) {
    return;
  }
  int i = index % MATRIX_CODE_GLYPH_COUNT;
  if (i < 0) {
    i += MATRIX_CODE_GLYPH_COUNT;
  }
  memcpy_P(out, &MATRIX_CODE_GLYPHS[i], sizeof(MatrixGlyph));
}

#endif