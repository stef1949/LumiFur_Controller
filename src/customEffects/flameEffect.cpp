// flameEffect.cpp
#include "customEffects/flameEffect.h"
#include <algorithm>
#include <new>

// --- Static (file-scope) Variables for the Flame Effect ---
static DisplayOutputType *g_display_ptr = nullptr; // Pointer to the display object
static uint8_t *g_heat_buffer = nullptr;           // 1D array storing heat values for each pixel
static uint16_t *g_scanline_buffer = nullptr;      // Temporary row buffer for RGB565 output
static int32_t g_display_width = 0;                    // Width of the display
static int32_t g_display_height = 0;                   // Height of the display

namespace
{
  inline int32_t wrapColumn(int32_t x)
  {
    if (x < 0)
    {
      x += g_display_width;
    }
    else if (x >= g_display_width)
    {
      x -= g_display_width;
    }
    return x;
  }

  inline uint8_t &heatAt(int32_t x, int32_t y)
  {
    return g_heat_buffer[y * g_display_width + x];
  }

  void freeFlameBuffers()
  {
    delete[] g_heat_buffer;
    g_heat_buffer = nullptr;
    delete[] g_scanline_buffer;
    g_scanline_buffer = nullptr;
  }
} // namespace

// --- Flame Color Palette ---
// Define a FastLED-style gradient palette for the flame colors.
// Black -> Dark Red -> Red/Orange -> Bright Yellow -> White-ish Yellow
DEFINE_GRADIENT_PALETTE( fire_gradient_palette_gp ) {
    0,   0,  0,  0,   // Black
   60, 100,  0,  0,   // Dark Red (index 60)
  120, 200, 40,  0,   // Red/Orange (index 120)
  180, 255,150,  0,   // Bright Orange/Yellow (index 180)
  255, 255,255,220  // Very Bright Yellow/White-ish (index 255)
};
CRGBPalette16 g_flame_palette = fire_gradient_palette_gp;


// --- Function Implementations ---

void initFlameEffect(DisplayOutputType *display)
{
  g_display_ptr = display;
  if (!g_display_ptr)
  {
    Serial.println("FlameEffect Error: Display pointer is null.");
    return;
  }

  g_display_width = g_display_ptr->width();
  g_display_height = g_display_ptr->height();

  if (g_display_width <= 0 || g_display_height <= 0)
  {
    Serial.println("FlameEffect Error: Invalid display dimensions.");
    return;
  }

  // Clean up old buffers if any (e.g., if re-initializing)
  freeFlameBuffers();

  // Allocate memory for the heat and scanline buffers
  const size_t heat_buffer_size = static_cast<size_t>(g_display_width) * g_display_height;
  g_heat_buffer = new (std::nothrow) uint8_t[heat_buffer_size];
  g_scanline_buffer = new (std::nothrow) uint16_t[g_display_width];

  if (!g_heat_buffer || !g_scanline_buffer)
  {
    Serial.println("FlameEffect Error: Failed to allocate memory.");
    freeFlameBuffers();
    return;
  }

  memset(g_heat_buffer, 0, heat_buffer_size * sizeof(uint8_t));
  memset(g_scanline_buffer, 0, g_display_width * sizeof(uint16_t));

  Serial.printf("Flame effect initialized for %dx%d display.\n", g_display_width, g_display_height);
}

void updateAndDrawFlameEffect()
{
  if (!g_display_ptr || !g_heat_buffer || !g_scanline_buffer)
  {
    // Not initialized or error during initialization
    return;
  }

  // Cooling/heat propagation is adapted from the FastLED Fire2012 example by Mark Kriegsman.
  random16_add_entropy(random16());

  const int total_cells = g_display_width * g_display_height;
  const uint8_t cooling_range = static_cast<uint8_t>(((FLAME_BASE_COOLING_MAX_RANDOM * 10) / std::max(1, g_display_height)) + 2);

  // Step 1: Cool down every cell slightly (Fire2012-style cooling).
  for (int i = 0; i < total_cells; ++i)
  {
    g_heat_buffer[i] = qsub8(g_heat_buffer[i], random8(0, cooling_range));
  }

  // Step 2: Propagate heat upward within each column, with a tiny horizontal drift.
  for (int x = 0; x < g_display_width; ++x)
  {
    for (int y = g_display_height - 1; y >= 2; --y)
    {
      const int drift = static_cast<int>(random8(3)) - 1; // -1, 0, or +1
      const int neighbor_column = wrapColumn(x + drift);

      uint16_t new_heat = heatAt(neighbor_column, y - 1);
      new_heat += heatAt(x, y - 1);
      new_heat += heatAt(x, y - 2);
      heatAt(x, y) = static_cast<uint8_t>(new_heat / 3);
    }
  }

  // Step 3: Generate new sparks near the bottom rows to keep the flames alive.
  for (int x = 0; x < g_display_width; ++x)
  {
    if (random8() < FLAME_SPARKING_CHANCE)
    {
      const int spawn_row = g_display_height - 1 - random8(0, std::min(2, g_display_height));
      const uint8_t spark_heat = FLAME_SPARK_MIN_HEAT + random8(FLAME_SPARK_MAX_HEAT - FLAME_SPARK_MIN_HEAT + 1);
      uint8_t &cell = heatAt(x, spawn_row);
      cell = qadd8(cell, spark_heat);
    }
  }

  // Step 4: Draw the flame buffer to the display.
  // A slight horizontal blur during rendering removes harsh column artifacts.
  for (int y = 0; y < g_display_height; ++y)
  {
    for (int x = 0; x < g_display_width; ++x)
    {
      uint8_t heat_value = heatAt(x, y);
      if (g_display_width > 2)
      {
        const uint8_t left = heatAt(wrapColumn(x - 1), y);
        const uint8_t right = heatAt(wrapColumn(x + 1), y);
        heat_value = static_cast<uint8_t>((heat_value * 2 + left + right) >> 2);
      }

      const CRGB color = ColorFromPalette(g_flame_palette, heat_value, 255, LINEARBLEND);
      g_scanline_buffer[x] = g_display_ptr->color565(color.r, color.g, color.b);
    }
    g_display_ptr->drawRGBBitmap(0, y, g_scanline_buffer, g_display_width, 1);
  }
}
