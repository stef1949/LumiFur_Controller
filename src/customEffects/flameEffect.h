// flameEffect.h
#ifndef FLAME_EFFECT_H
#define FLAME_EFFECT_H

#include <Arduino.h>
#include <FastLED.h> // For CRGB, CHSV types


// We need to know the type of the display object.
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
typedef VirtualMatrixPanel DisplayOutputType; 
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
typedef MatrixPanel_I2S_DMA DisplayOutputType; 
#endif


// --- Flame Effect Parameters (tune these for desired appearance) ---

// Overall cooling: A base amount of heat subtracted from each pixel each frame.
// This causes flames to die down.
// Value is max random cooling from 0 to this value.
// e.g., 3 means each pixel cools by random8(0, 3+1) -> random value between 0 and 3.
#define FLAME_BASE_COOLING_MAX_RANDOM 2 

// Spark generation at the bottom of the display:
#define FLAME_SPARKING_CHANCE 100  // Chance (out of 255) of a new spark appearing in a column.
#define FLAME_SPARK_MIN_HEAT  170  // Minimum heat value for a new spark (0-255).
#define FLAME_SPARK_MAX_HEAT  255  // Maximum heat value for a new spark (0-255).

// --- Function Declarations ---

/**
 * @brief Initializes the flame effect.
 * Call this once in your setup() function.
 * @param display Pointer to your Adafruit_GFX compatible display object (e.g., dma_display).
 */
void initFlameEffect(DisplayOutputType* display);

/**
 * @brief Updates and draws one frame of the flame effect.
 * Call this in your main loop when the flame effect is active.
 */
void updateAndDrawFlameEffect();

#endif // FLAME_EFFECT_H