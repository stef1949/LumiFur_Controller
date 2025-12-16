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
    if (x < 0) x += g_display_width;
    else if (x >= g_display_width) x -= g_display_width;
    return x;
  }

  // SAFE ACCESSOR: 
  // Includes bounds checking to prevent ESP32 "LoadProhibited" Panics
  inline uint8_t &heatAt(int32_t x, int32_t y)
  {
    // Clamp coordinates to valid range. 
    // This prevents a crash if the simulation logic drifts out of bounds.
    if (x < 0) x = 0;
    else if (x >= g_display_width) x = g_display_width - 1;
    
    if (y < 0) y = 0;
    else if (y >= g_display_height) y = g_display_height - 1;

    // Use size_t for array indexing
    return g_heat_buffer[(size_t)y * (size_t)g_display_width + (size_t)x];
  }

  void freeFlameBuffers()
  {
    if (g_heat_buffer) {
        delete[] g_heat_buffer;
        g_heat_buffer = nullptr;
    }
    if (g_scanline_buffer) {
        delete[] g_scanline_buffer;
        g_scanline_buffer = nullptr;
    }
  }
} // namespace

// --- Flame Color Palette ---
DEFINE_GRADIENT_PALETTE( fire_gradient_palette_gp ) {
    0,   0,  0,  0,   // Black
   60, 100,  0,  0,   // Dark Red
  120, 200, 40,  0,   // Red/Orange
  180, 255,150,  0,   // Bright Orange/Yellow
  255, 255,255,220    // White-ish
};
CRGBPalette16 g_flame_palette = fire_gradient_palette_gp;

// --- Function Implementations ---

void initFlameEffect(DisplayOutputType *display)
{
  g_display_ptr = display;
  if (!g_display_ptr) return;

  g_display_width = (int32_t)g_display_ptr->width();
  g_display_height = (int32_t)g_display_ptr->height();

  if (g_display_width <= 0 || g_display_height <= 0) return;

  freeFlameBuffers();

  // Allocation with std::nothrow prevents crashing if RAM is full
  // (Common on ESP32 if WiFi/Bluetooth stacks are also running)
  size_t total_pixels = (size_t)g_display_width * (size_t)g_display_height;
  g_heat_buffer = new (std::nothrow) uint8_t[total_pixels];
  g_scanline_buffer = new (std::nothrow) uint16_t[g_display_width];

  if (!g_heat_buffer || !g_scanline_buffer)
  {
    Serial.println("FlameEffect: RAM Allocation Failed!");
    freeFlameBuffers();
    return;
  }

  memset(g_heat_buffer, 0, total_pixels);
  memset(g_scanline_buffer, 0, g_display_width * sizeof(uint16_t));
  
  Serial.printf("Flame initialized: %dx%d\n", g_display_width, g_display_height);
}

void updateAndDrawFlameEffect()
{
  if (!g_display_ptr || !g_heat_buffer || !g_scanline_buffer) return;

  // SAFETY: Check if rotation changed the dimensions.
  // If we draw to a 64x32 buffer but the screen is now 32x64, we will crash.
  if ((int32_t)g_display_ptr->width() != g_display_width || 
      (int32_t)g_display_ptr->height() != g_display_height) {
      return; 
  }

  random16_add_entropy(random16());

  const size_t total_cells = (size_t)g_display_width * (size_t)g_display_height;
  
  // Adjusted cooling for larger matricies (like 64x64)
  uint8_t cooling = (FLAME_BASE_COOLING_MAX_RANDOM * 10) / std::max(1, (int)g_display_height) + 2;

  // Step 1: Cool down
  for (size_t i = 0; i < total_cells; ++i) {
    g_heat_buffer[i] = qsub8(g_heat_buffer[i], random8(0, cooling));
  }

  // Step 2: Propagate heat
  for (int32_t x = 0; x < g_display_width; ++x)
  {
    for (int32_t y = g_display_height - 1; y >= 2; --y)
    {
      int32_t drift = (int32_t)random8(3) - 1; 
      int32_t neighbor = wrapColumn(x + drift);

      uint16_t h = heatAt(neighbor, y - 1);
      h += heatAt(x, y - 1);
      h += heatAt(x, y - 2);
      
      heatAt(x, y) = (uint8_t)(h / 3);
    }
  }

  // Step 3: Sparking
  for (int32_t x = 0; x < g_display_width; ++x)
  {
    if (random8() < FLAME_SPARKING_CHANCE)
    {
      int32_t y_pos = g_display_height - 1 - random8(0, std::min((int32_t)2, g_display_height));
      // qadd8 ensures we don't overflow uint8_t (stops at 255)
      heatAt(x, y_pos) = qadd8(heatAt(x, y_pos), random8(FLAME_SPARK_MIN_HEAT, FLAME_SPARK_MAX_HEAT));
    }
  }

  // Step 4: Draw
  for (int32_t y = 0; y < g_display_height; ++y)
  {
    for (int32_t x = 0; x < g_display_width; ++x)
    {
      uint8_t heat = heatAt(x, y);
      
      // Horizontal blur
      if (g_display_width > 2)
      {
        uint8_t left = heatAt(wrapColumn(x - 1), y);
        uint8_t right = heatAt(wrapColumn(x + 1), y);
        heat = (uint8_t)(( (uint16_t)heat * 2 + left + right) >> 2);
      }

      CRGB color = ColorFromPalette(g_flame_palette, heat, 255, LINEARBLEND);
      g_scanline_buffer[x] = g_display_ptr->color565(color.r, color.g, color.b);
    }
    
    g_display_ptr->drawRGBBitmap(0, y, g_scanline_buffer, g_display_width, 1);

    // CRITICAL FOR ESP32: Yield every few rows to prevent Watchdog Timeout
    // if the drawing operation is slow or blocking.
    if (y % 8 == 0) yield();
 }
}