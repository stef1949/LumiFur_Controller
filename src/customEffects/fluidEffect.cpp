#include "fluidEffect.h"
#include <Arduino.h>

#define PANE_WIDTH 64
#define PANE_HEIGHT 32

// Constructor
FluidEffect::FluidEffect(MatrixDisplayAdaptor* display, Adafruit_LIS3DH* accel, bool* accelEnabled)
    : dma_display(display), accelerometer(accel), accelerometer_enabled(accelEnabled), num_active_particles(0) {
    // Seed random number generator if not done globally, or if specific seed needed
    // randomSeed(analogRead(0)); // Usually done in main setup()
}

// Destructor
FluidEffect::~FluidEffect() {
    // Nothing specific to clean up with this simple struct array
}

// Called when the effect starts
void FluidEffect::begin() {
    initializeParticles();
}

void FluidEffect::initializeParticles() {
    num_active_particles = MAX_PARTICLES_FLUID; // Use all particles
    if (!dma_display) return; // Should not happen if setup correctly

    for (int i = 0; i < num_active_particles; ++i) {
        // Initial position: a block of particles in the upper middle part of the screen
        particles[i].x = random(PANE_WIDTH / 3, 2 * PANE_WIDTH / 3);
        particles[i].y = random(PANE_HEIGHT / 4, PANE_HEIGHT / 2);

        // Initial velocity: small random velocities
        particles[i].xv = ((float)random(-50, 51) / 100.0f) * 0.5f; // Random between -0.25 and +0.25
        particles[i].yv = ((float)random(-50, 51) / 100.0f) * 0.5f; // Random between -0.25 and +0.25
    }
}

void FluidEffect::applyGravityAndAccel() {
    float current_gravity_x = 0.0f;
    float current_gravity_y = GRAVITY_BASE; // Default gravity downwards

    // Check if accelerometer is enabled and available
    if (accelerometer_enabled && *accelerometer_enabled && accelerometer) {
        sensors_event_t event;
        if (accelerometer->getEvent(&event)) {
            // Adjust "gravity" based on accelerometer readings
            // event.acceleration.x, y, z are in m/s^2
            // Assuming panel is held upright, or laid flat.
            // If laid flat (screen facing up):
            // Tilt left/right along X-axis: event.acceleration.x
            // Tilt forward/backward along Y-axis: event.acceleration.y
            // These factors might need tuning based on how panel is oriented
            current_gravity_x += event.acceleration.x * ACCEL_SENSITIVITY_MULTIPLIER;
            current_gravity_y -= event.acceleration.y * ACCEL_SENSITIVITY_MULTIPLIER; // Common for Y to be inverted
        }
    }

    for (int i = 0; i < num_active_particles; ++i) {
        particles[i].xv += current_gravity_x * DT_FLUID;
        particles[i].yv += current_gravity_y * DT_FLUID;
    }
}

void FluidEffect::applyParticleInteractions() {
    for (int i = 0; i < num_active_particles; ++i) {
        for (int j = i + 1; j < num_active_particles; ++j) { // Iterate j from i+1 to avoid redundant pairs
            float dx = particles[j].x - particles[i].x;
            float dy = particles[j].y - particles[i].y;
            float dist_sq = dx * dx + dy * dy;

            // Check if particles are close enough to interact and not at the exact same spot
            if (dist_sq < PARTICLE_INTERACTION_DISTANCE_SQ && dist_sq > 0.0001f) {
                float dist = sqrt(dist_sq);
                
                // Repulsive force = factor * (overlap_distance / distance)
                // Force is stronger for more overlap and closer particles
                float force_magnitude = PARTICLE_REPEL_FACTOR * (PARTICLE_INTERACTION_DISTANCE - dist) / dist;
                
                float force_x_component = dx * force_magnitude;
                float force_y_component = dy * force_magnitude;

                // Apply force to both particles (equal and opposite)
                particles[i].xv -= force_x_component * DT_FLUID;
                particles[i].yv -= force_y_component * DT_FLUID;
                particles[j].xv += force_x_component * DT_FLUID;
                particles[j].yv += force_y_component * DT_FLUID;
            }
        }
    }
}

void FluidEffect::updateParticlePositions() {
    for (int i = 0; i < num_active_particles; ++i) {
        // Apply drag
        particles[i].xv *= PARTICLE_DRAG;
        particles[i].yv *= PARTICLE_DRAG;

        // Update positions
        particles[i].x += particles[i].xv * DT_FLUID;
        particles[i].y += particles[i].yv * DT_FLUID;
    }
}

void FluidEffect::applyScreenCollisions() {
    if (!dma_display) return;

    for (int i = 0; i < num_active_particles; ++i) {
        // Collision with left wall
        if (particles[i].x < 0) {
            particles[i].x = 0;
            particles[i].xv *= WALL_DAMPING;
        }
        // Collision with right wall
        else if (particles[i].x >= PANE_WIDTH) {
            particles[i].x = PANE_WIDTH - 1.0f; // Ensure it's within bounds
            particles[i].xv *= WALL_DAMPING;
        }

        // Collision with top wall
        if (particles[i].y < 0) {
            particles[i].y = 0;
            particles[i].yv *= WALL_DAMPING;
        }
        // Collision with bottom wall
        else if (particles[i].y >= PANE_HEIGHT) {
            particles[i].y = PANE_HEIGHT - 1.0f; // Ensure it's within bounds
            particles[i].yv *= WALL_DAMPING;
        }
    }
}

void FluidEffect::drawParticles() {
    if (!dma_display) return;

    // dma_display->clearScreen(); // This is typically handled by the main display loop before calling updateAndDraw()

    for (int i = 0; i < num_active_particles; ++i) {
        // Determine particle color (e.g., based on speed or fixed)
        float speed_sq = particles[i].xv * particles[i].xv + particles[i].yv * particles[i].yv;
        
        // Map speed to brightness/color. Max expected speed_sq might be ~2.0f for these parameters.
        int blue_intensity = map(constrain(speed_sq, 0.0f, 2.0f) * 100, 0, 200, 100, 255); // Base blue intensity
        int green_intensity = map(constrain(speed_sq, 0.0f, 2.0f) * 100, 0, 200, 50, 150); // Mix in some green for highlights

        uint16_t particle_color = dma_display->color565(0, green_intensity, blue_intensity); // Shades of cyan/blue

        // Draw the particle
        // Ensure coordinates are integers and within bounds before drawing, though collisions should handle it.
        int px = static_cast<int>(particles[i].x);
        int py = static_cast<int>(particles[i].y);

        if (px >= 0 && px < PANE_WIDTH && py >= 0 && py < PANE_HEIGHT) {
            dma_display->drawPixel(px, py, particle_color);
            // For slightly larger particles, you could use fillRect:
            // dma_display->fillRect(px > 0 ? px -1 : 0, py > 0 ? py -1 : 0, 2, 2, particle_color);
        }
    }
}

// Main update and draw call for the effect
void FluidEffect::updateAndDraw() {
    if (!dma_display) return;

    // Simulation steps
    applyGravityAndAccel();
    applyParticleInteractions(); // This is the most computationally expensive step
    updateParticlePositions();
    applyScreenCollisions();

    // Rendering
    // The main loop already clears the screen via dma_display->clearScreen();
    drawParticles();
    // The main loop handles dma_display->flipDMABuffer();
}