#ifndef FLUID_EFFECT_H
#define FLUID_EFFECT_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <vector> // For storing particles

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
#include <Adafruit_LIS3DH.h>
#endif

struct Particle {
    float x, y;     // Position (can be sub-pixel)
    float vx, vy;   // Velocity
    uint16_t color; // Optional: for different particle types or effects
    // float radius; // Optional for collision
};

class FluidEffect {
public:
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
    FluidEffect(MatrixPanel_I2S_DMA* display, Adafruit_LIS3DH* accelerometer, bool* accelEnabledGlobalFlag);
#else
    FluidEffect(MatrixPanel_I2S_DMA* display, void* dummy_accelerometer_placeholder, bool* accelEnabledGlobalFlag);
#endif
    // ~FluidEffect(); // std::vector will handle its own memory

    void begin();
    void updateAndDraw();

private:
    MatrixPanel_I2S_DMA* dma_display_;
    bool* accelerometerEnabledGlobalFlag_;

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
    Adafruit_LIS3DH* accel_;
#endif

    float smoothedAccelX_g_;
    float smoothedAccelY_g_; // We'll use Y-axis accelerometer for "up/down" gravity now

    std::vector<Particle> particles_;
    int gridWidth_;
    int gridHeight_;

    // --- Simulation Parameters ---
    unsigned long lastSimUpdateTime_;
    static const unsigned long SIM_UPDATE_INTERVAL_MS;
    static const int MAX_PARTICLES;
    static const float PARTICLE_RADIUS; // For drawing and simple collision
    static const float GRAVITY_Y;
    static const float ACCEL_FORCE_FACTOR;
    static const float BOUNDARY_DAMPING; // Energy loss on collision (0 to 1)
    static const float DRAG_COEFFICIENT; // Simple air resistance/viscosity

    // --- Private Helper Methods ---
    void initializeParticles();
    void applyGlobalForces(float dt);
    void updateParticlePositions(float dt);
    void handleBoundaryCollisions();
    // void handleInterParticleCollisions(); // SKIPPING for simplicity initially
    void renderParticles();
    void addParticle(float x, float y, float vx = 0, float vy = 0);
    void showMessage(const char* msg, uint16_t color);
};

#endif // FLUID_EFFECT_H