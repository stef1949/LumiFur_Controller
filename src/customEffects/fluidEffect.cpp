#include "fluidEffect.h"
#include "Fonts/TomThumb.h" // Ensure this path is correct

// Includes for panel and accelerometer
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
#include <Adafruit_LIS3DH.h>
#endif

// --- Simulation Parameters ---
const unsigned long FluidEffect::SIM_UPDATE_INTERVAL_MS = 30; // Faster update for particle dynamics (adjust)
const int FluidEffect::MAX_PARTICLES = 5000;                  // Max number of particles (adjust for performance)
const float FluidEffect::PARTICLE_RADIUS = 1.0f;             // For rendering as 1x1 pixel if centered
const float FluidEffect::GRAVITY_Y = 9.8f * 2.0f;            // Scaled gravity (pixels/sec^2), adjust strength
const float FluidEffect::ACCEL_FORCE_FACTOR = 15.0f;         // How much accelerometer influences particles
const float FluidEffect::BOUNDARY_DAMPING = 0.6f;            // Energy lost on wall collision (0.0=no bounce, 1.0=perfect bounce)
const float FluidEffect::DRAG_COEFFICIENT = 0.01f;           // Slows down particles over time

// --- Constructor ---
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
FluidEffect::FluidEffect(MatrixPanel_I2S_DMA* display, Adafruit_LIS3DH* accelerometer, bool* accelEnabledGlobalFlag)
    : dma_display_(display),
      accelerometerEnabledGlobalFlag_(accelEnabledGlobalFlag),
      accel_(accelerometer),
      smoothedAccelX_g_(0.0f), smoothedAccelY_g_(0.0f),
      gridWidth_(0), gridHeight_(0),
      lastSimUpdateTime_(0) {
    if (dma_display_) {
        gridWidth_ = dma_display_->width();
        gridHeight_ = dma_display_->height();
        particles_.reserve(MAX_PARTICLES); // Pre-allocate vector capacity
    } else { Serial.println("FluidEffect ERROR: Display pointer is null!"); }
    if (!accel_) { Serial.println("FluidEffect WARNING: Accel pointer is null!"); }
    if (!accelerometerEnabledGlobalFlag_) { Serial.println("FluidEffect ERROR: accelEnabledGlobalFlag is null!"); }
}
#else
FluidEffect::FluidEffect(MatrixPanel_I2S_DMA* display, void* /*dummy_accel*/, bool* accelEnabledGlobalFlag)
    : dma_display_(display),
      accelerometerEnabledGlobalFlag_(accelEnabledGlobalFlag),
      // accel_ is not initialized
      smoothedAccelX_g_(0.0f), smoothedAccelY_g_(0.0f),
      gridWidth_(0), gridHeight_(0),
      lastSimUpdateTime_(0) {
    if (dma_display_) {
        gridWidth_ = dma_display_->width();
        gridHeight_ = dma_display_->height();
        particles_.reserve(MAX_PARTICLES);
    } else { Serial.println("FluidEffect ERROR: Display pointer is null!"); }
    if (!accelerometerEnabledGlobalFlag_) { Serial.println("FluidEffect ERROR: accelEnabledGlobalFlag is null!"); }
}
#endif

// FluidEffect::~FluidEffect() { /* particles_ vector handles its own memory */ }

void FluidEffect::addParticle(float x, float y, float vx, float vy) {
    if (particles_.size() < MAX_PARTICLES) {
        Particle p = {x, y, vx, vy, dma_display_->color565(60, 120, 255)}; // Default blue
        // Particle p = {x, y, vx, vy, dma_display_->color565(random(50,100), random(100,150), random(200,255))}; // Random blue shades
        particles_.push_back(p);
    }
}

void FluidEffect::initializeParticles() {
    particles_.clear();
    if (!dma_display_ || gridWidth_ == 0 || gridHeight_ == 0) return;

    int initialParticleCount = MAX_PARTICLES * 0.8; // Start with 80% of max particles
    for (int i = 0; i < initialParticleCount; ++i) {
        // Place particles in the bottom half, somewhat randomly
        float px = random(gridWidth_ * 0.1f, gridWidth_ * 0.9f);
        float py = random(gridHeight_ * 0.5f, gridHeight_ * 0.9f); // Start in bottom half
        addParticle(px, py);
    }
    Serial.printf("Initialized with %d particles.\n", particles_.size());
}

void FluidEffect::begin() {
    initializeParticles();
    smoothedAccelX_g_ = 0.0f;
    smoothedAccelY_g_ = 0.0f; // For accelerometer Y-axis
    lastSimUpdateTime_ = millis();

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
    if (accel_ && accelerometerEnabledGlobalFlag_ && *accelerometerEnabledGlobalFlag_) {
        accel_->read();
        smoothedAccelX_g_ = accel_->x_g;
        smoothedAccelY_g_ = accel_->y_g; // Use Y-axis accelerometer reading
    }
#endif
    Serial.println("FluidEffect::begin() - Particle System Initialized");
}


void FluidEffect::applyGlobalForces(float dt) {
    // Accelerometer X for horizontal force
    // Accelerometer Y for vertical force (counteracting or adding to gravity)
    // Note: accel_->y_g might be positive when tilted "up" or "down" depending on orientation.
    // If panel is flat, Y might be near 0. If vertical, Y might be +/-1G.
    // We'll assume Y-axis of accel corresponds to screen's Y-axis for simplicity.
    float forceX = smoothedAccelX_g_ * ACCEL_FORCE_FACTOR;
    float forceY = GRAVITY_Y - (smoothedAccelY_g_ * ACCEL_FORCE_FACTOR); // Gravity pulls down, accel Y can push up/down

    for (auto& p : particles_) {
        p.vx += forceX * dt;
        p.vy += forceY * dt;

        // Apply drag
        p.vx *= (1.0f - DRAG_COEFFICIENT);
        p.vy *= (1.0f - DRAG_COEFFICIENT);
    }
}

void FluidEffect::updateParticlePositions(float dt) {
    for (auto& p : particles_) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
}

void FluidEffect::handleBoundaryCollisions() {
    if (gridWidth_ == 0 || gridHeight_ == 0) return;
    // static unsigned long lastCollisionPrint = 0; // To avoid spamming

    for (auto& p : particles_) {
        bool collidedX = false;
        bool collidedY = false;

        // Left boundary
        if (p.x - PARTICLE_RADIUS < 0) {
            // if (millis() - lastCollisionPrint > 1000) { // Print only once a second
            //     Serial.printf("L_COL: P(%.1f,%.1f) V(%.1f,%.1f) -> ", p.x, p.y, p.vx, p.vy);
            // }
            p.x = PARTICLE_RADIUS;
            p.vx *= -BOUNDARY_DAMPING;
            collidedX = true;
            // if (millis() - lastCollisionPrint > 1000) {
            //     Serial.printf("P(%.1f,%.1f) V(%.1f,%.1f)\n", p.x, p.y, p.vx, p.vy);
            // }
        }
        // Right boundary
        else if (p.x + PARTICLE_RADIUS >= gridWidth_) { // Use >= for the upper bound check with floats
            // if (millis() - lastCollisionPrint > 1000) {
            //     Serial.printf("R_COL: P(%.1f,%.1f) V(%.1f,%.1f) -> ", p.x, p.y, p.vx, p.vy);
            // }
            p.x = gridWidth_ - 1.0f - PARTICLE_RADIUS; // Ensure rightmost pixel is gridWidth - 1
            p.vx *= -BOUNDARY_DAMPING;
            collidedX = true;
            // if (millis() - lastCollisionPrint > 1000) {
            //      Serial.printf("P(%.1f,%.1f) V(%.1f,%.1f)\n", p.x, p.y, p.vx, p.vy);
            // }
        }

        // Top boundary
        if (p.y - PARTICLE_RADIUS < 0) {
            // if (millis() - lastCollisionPrint > 1000) {
            //     Serial.printf("T_COL: P(%.1f,%.1f) V(%.1f,%.1f) -> ", p.x, p.y, p.vx, p.vy);
            // }
            p.y = PARTICLE_RADIUS;
            p.vy *= -BOUNDARY_DAMPING;
            collidedY = true;
            // if (millis() - lastCollisionPrint > 1000) {
            //     Serial.printf("P(%.1f,%.1f) V(%.1f,%.1f)\n", p.x, p.y, p.vx, p.vy);
            // }
        }
        // Bottom boundary
        else if (p.y + PARTICLE_RADIUS >= gridHeight_) { // Use >= for the upper bound check
            // if (millis() - lastCollisionPrint > 1000) {
            //     Serial.printf("B_COL: P(%.1f,%.1f) V(%.1f,%.1f) -> ", p.x, p.y, p.vx, p.vy);
            // }
            p.y = gridHeight_ - 1.0f - PARTICLE_RADIUS; // Ensure bottommost pixel is gridHeight - 1
            p.vy *= -BOUNDARY_DAMPING;
            collidedY = true;
            // if (millis() - lastCollisionPrint > 1000) {
            //     Serial.printf("P(%.1f,%.1f) V(%.1f,%.1f)\n", p.x, p.y, p.vx, p.vy);
            //     lastCollisionPrint = millis();
            // }
        }
    }
}

// void FluidEffect::handleInterParticleCollisions() { /* TODO: More complex */ }

void FluidEffect::renderParticles() {
    if (!dma_display_) return;
    dma_display_->fillScreen(dma_display_->color565(10, 20, 40)); // Background

    for (const auto& p : particles_) {
        // Simple 1x1 pixel rendering
        int ix = round(p.x);
        int iy = round(p.y);
        if (ix >= 0 && ix < gridWidth_ && iy >= 0 && iy < gridHeight_) {
            dma_display_->drawPixel(ix, iy, p.color);
        }
        // Optional: draw slightly larger circles if PARTICLE_RADIUS > 0.5
        // else if (PARTICLE_RADIUS > 0.5f) {
        //    dma_display_->fillCircle(round(p.x), round(p.y), round(PARTICLE_RADIUS), p.color);
        // }
    }
}

void FluidEffect::updateAndDraw() {
    if (!dma_display_ || !accelerometerEnabledGlobalFlag_) {
        if (dma_display_) showMessage("Sys Err", dma_display_->color565(255,0,0));
        return;
    }

    float currentAccelX_g = 0.0f;
    float currentAccelY_g = 0.0f; // For screen Y-axis related forces

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
    if (!(*accelerometerEnabledGlobalFlag_)) {
        showMessage("Accel OFF", dma_display_->color565(150, 150, 150));
        // Simulate with no external accel forces if accel is off by user
    } else {
        if (!accel_) {
            showMessage("Accel ERR", dma_display_->color565(255, 100, 0));
            // Simulate with no external accel forces
        } else {
            accel_->read();
            currentAccelX_g = accel_->x_g;
            currentAccelY_g = accel_->y_g; // Read Y for vertical screen forces
        }
    }
    // Smooth accelerometer readings
    smoothedAccelX_g_ = (0.2f * currentAccelX_g) + (0.8f * smoothedAccelX_g_);
    smoothedAccelY_g_ = (0.2f * currentAccelY_g) + (0.8f * smoothedAccelY_g_);

#else // No accelerometer hardware compiled
    // smoothedAccelX_g_ and smoothedAccelY_g_ remain 0.0f
#endif

    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - lastSimUpdateTime_;

    if (elapsedTime >= SIM_UPDATE_INTERVAL_MS) {
        float dt = elapsedTime / 1000.0f; // Delta time in seconds
        // Clamp dt to avoid explosions if there's a large lag
        if (dt > 0.1f) dt = 0.1f; 

        applyGlobalForces(dt);
        updateParticlePositions(dt);
        handleBoundaryCollisions();
        // handleInterParticleCollisions(); // If implemented

        lastSimUpdateTime_ = currentTime;
    }

    renderParticles();
}

void FluidEffect::showMessage(const char* msg, uint16_t color) {
    if (!dma_display_) return;
    dma_display_->fillScreen(dma_display_->color565(0, 0, 0));
    dma_display_->setFont(&TomThumb);
    dma_display_->setTextSize(1);
    dma_display_->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    dma_display_->getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    dma_display_->setCursor((dma_display_->width() - w) / 2, (dma_display_->height() / 2) + (h / 2) -1);
    dma_display_->print(msg);
}