// flameEffect.cpp
#include "customEffects/flameEffect.h"
#include <algorithm> // For std::max, std::min

#include <Arduino.h> // For random() if FastLED's random8() isn't available, though it should be.

// --- Static (file-scope) Variables for the Flame Effect ---
static DisplayOutputType* g_display_ptr = nullptr;   // Pointer to the display object
static uint8_t* g_heat_buffer = nullptr;      // 1D array storing heat values for each pixel
static int g_display_width;                   // Width of the display
static int g_display_height;                  // Height of the display

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

void initFlameEffect(DisplayOutputType* display) {
    g_display_ptr = display;
    if (!g_display_ptr) {
        Serial.println("FlameEffect Error: Display pointer is null.");
        return;
    }

    g_display_width = g_display_ptr->width();
    g_display_height = g_display_ptr->height();

    if (g_display_width <= 0 || g_display_height <= 0) {
        Serial.println("FlameEffect Error: Invalid display dimensions.");
        return;
    }

    // Clean up old buffer if any (e.g., if re-initializing)
    if (g_heat_buffer) {
        delete[] g_heat_buffer;
        g_heat_buffer = nullptr;
    }

    // Allocate memory for the heat buffer
    size_t buffer_size = (size_t)g_display_width * g_display_height;
    g_heat_buffer = new uint8_t[buffer_size];
    
    if (!g_heat_buffer) {
        Serial.println("FlameEffect Error: Failed to allocate memory for heat buffer!");
        return;
    }

    // Initialize heat buffer to all zeros (cold)
    memset(g_heat_buffer, 0, buffer_size * sizeof(uint8_t));
    
    Serial.printf("Flame effect initialized for %dx%d display.\n", g_display_width, g_display_height);
}

void updateAndDrawFlameEffect() {
    if (!g_display_ptr || !g_heat_buffer) {
        // Not initialized or error during initialization
        return;
    }

    // Step 1: Cool down every cell slightly.
    // This prevents heat from accumulating indefinitely and makes flames die out.
    for (int i = 0; i < g_display_width * g_display_height; i++) {
        g_heat_buffer[i] = qsub8(g_heat_buffer[i], random8(0, FLAME_BASE_COOLING_MAX_RANDOM + 1));
    }

    // Step 2: Propagate heat upwards and spread.
    // Iterate from the second to last row (y = g_display_height - 2) up to the top row (y = 0).
    // Heat for a pixel heat[x][y] is calculated based on the heat of pixels in rows below it (y+1, y+2).
    for (int y = g_display_height - 2; y >= 0; y--) {
        for (int x = 0; x < g_display_width; x++) {
            int current_pixel_idx = x + y * g_display_width;

            // Sum heat from 3 pixels in the row below (y+1) and 1 pixel from two rows below (y+2).
            // This creates an upward and outward spread of heat.
            uint32_t heat_sum = 0;
            
            // Pixel directly below, weighted more for stronger upward motion
            heat_sum += g_heat_buffer[x + (y + 1) * g_display_width] * 2; 
            // Pixel below-left
            heat_sum += g_heat_buffer[((x - 1 + g_display_width) % g_display_width) + (y + 1) * g_display_width]; 
            // Pixel below-right
            heat_sum += g_heat_buffer[((x + 1) % g_display_width) + (y + 1) * g_display_width];           

            // If not at the second to last row, also consider heat from two rows below.
            if (y < g_display_height - 2) {
                heat_sum += g_heat_buffer[x + (y + 2) * g_display_width];                                  
                g_heat_buffer[current_pixel_idx] = (uint8_t)(heat_sum / 5.15f); // Average of 5 "effective" samples, division >5 cools it.
            } else { 
                // For the row just above the bottom, (y+2) is out of bounds. Average 4 "effective" samples.
                g_heat_buffer[current_pixel_idx] = (uint8_t)(heat_sum / 4.1f); // Division >4 cools it.
            }
        }
    }

    // Step 3: Generate new "sparks" at the bottom row (y = g_display_height - 1).
    // These are the "source" of the flame.
    for (int x = 0; x < g_display_width; x++) {
        if (random8() < FLAME_SPARKING_CHANCE) {
            int bottom_row_idx = x + (g_display_height - 1) * g_display_width;
            uint8_t spark_heat = FLAME_SPARK_MIN_HEAT + random8(FLAME_SPARK_MAX_HEAT - FLAME_SPARK_MIN_HEAT + 1);
            // Add heat to existing heat at the bottom, capped at 255.
            g_heat_buffer[bottom_row_idx] = qadd8(g_heat_buffer[bottom_row_idx], spark_heat); 
        }
    }

    // Step 4: Draw the flame buffer to the display.
    // Map heat values to colors using the flame palette.
    for (int y = 0; y < g_display_height; y++) {
        for (int x = 0; x < g_display_width; x++) {
            uint8_t heat_value = g_heat_buffer[x + y * g_display_width];
            // The 'brightness' parameter of ColorFromPalette is 255 for full palette brightness.
            // Overall display brightness is handled by dma_display->setBrightness8().
            CRGB color = ColorFromPalette(g_flame_palette, heat_value, 255, LINEARBLEND);
            g_display_ptr->drawPixel(x, y, ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3));
        }
    }
}