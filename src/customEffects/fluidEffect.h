#ifndef FLUID_EFFECT_H
#define FLUID_EFFECT_H

#include <Arduino.h>
#include <Adafruit_GFX.h>

// Determine which display class header to use based on VIRTUAL_PANE define
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
typedef VirtualMatrixPanel MatrixDisplayAdaptor; 
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
typedef MatrixPanel_I2S_DMA MatrixDisplayAdaptor; 
#endif

// Forward declare LIS3DH if it's used, or include its header
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE) // ACCEL_AVAILABLE from main.h
#include <Adafruit_LIS3DH.h>
#else
class Adafruit_LIS3DH; 
#endif

// PANE_WIDTH and PANE_HEIGHT will be determined by the display object's width() and height() methods.
// These defines are illustrative or for potential static array sizing if needed.
#ifndef PANE_WIDTH_DEFAULT_FLUID 
#define PANE_WIDTH_DEFAULT_FLUID 128 
#endif
#ifndef PANE_HEIGHT_DEFAULT_FLUID
#define PANE_HEIGHT_DEFAULT_FLUID 32
#endif


#define MAX_PARTICLES_FLUID 70 // Number of particles. Adjust for performance/look. (Was 80)

struct FluidParticle {
    float x, y;   // Position
    float xv, yv; // Velocity
};

class FluidEffect {
public:
    FluidEffect(MatrixDisplayAdaptor* display, Adafruit_LIS3DH* accel = nullptr, bool* accelEnabled = nullptr);
    ~FluidEffect();

    void begin();           // Called when the effect starts or is switched to
    void updateAndDraw();   // Called every frame to update simulation and draw

private:
    MatrixDisplayAdaptor* dma_display; // Renamed to avoid conflict with global dma_display if any scope issues
    Adafruit_LIS3DH* accelerometer; 
    bool* accelerometer_enabled;    

    FluidParticle particles[MAX_PARTICLES_FLUID];
    int num_active_particles;

    // Simulation parameters
    static constexpr float GRAVITY_BASE = 0.035f; // Slightly less for potentially smaller screens (was 0.04)
    static constexpr float PARTICLE_INTERACTION_DISTANCE = 6.0f; // (was 7.0)
    static constexpr float PARTICLE_INTERACTION_DISTANCE_SQ = PARTICLE_INTERACTION_DISTANCE * PARTICLE_INTERACTION_DISTANCE;
    static constexpr float PARTICLE_REPEL_FACTOR = 0.07f; // (was 0.08)
    static constexpr float PARTICLE_DRAG = 0.98f;         // (was 0.985)
    static constexpr float WALL_DAMPING = -0.35f;         // (was -0.4)
    static constexpr float ACCEL_SENSITIVITY_MULTIPLIER = 0.018f; // (was 0.015)
    static constexpr float DT_FLUID = 1.0f;              

    void initializeParticles();
    void applyGravityAndAccel();
    void applyParticleInteractions();
    void updateParticlePositions();
    void applyScreenCollisions();
    void drawParticles();
};

#endif // FLUID_EFFECT_H