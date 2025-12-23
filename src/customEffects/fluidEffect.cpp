#include "fluidEffect.h"
#include <Arduino.h>

extern bool getLatestAcceleration(float &x, float &y, float &z, const unsigned long sampleInterval);

// This effect uses a simple particle simulation:
// 1) Add gravity (optionally tilted by accelerometer input),
// 2) Apply short-range repulsion between particles,
// 3) Integrate velocity to position with drag,
// 4) Bounce off screen bounds with damping,
// 5) Render particles with speed-based color.

// Constructor
FluidEffect::FluidEffect(MatrixDisplayAdaptor* display, Adafruit_LIS3DH* accel, bool* accelEnabled)
    : dma_display(display),
      accelerometer(accel),
      accelerometer_enabled(accelEnabled),
      num_active_particles(0),
      pane_width(PANE_WIDTH_DEFAULT_FLUID),
      pane_height(PANE_HEIGHT_DEFAULT_FLUID),
      grid_cols(0),
      grid_rows(0),
      grid_cell_count(0) {
    // Seed random number generator if not done globally, or if specific seed needed
    // randomSeed(analogRead(0)); // Usually done in main setup()
}

// Destructor
FluidEffect::~FluidEffect() {
    // Nothing specific to clean up with this simple struct array
}

// Called when the effect starts
void FluidEffect::begin() {
    if (!dma_display) return;

    const int display_width = static_cast<int>(dma_display->width());
    const int display_height = static_cast<int>(dma_display->height());
    pane_width = (display_width > 0) ? display_width : PANE_WIDTH_DEFAULT_FLUID;
    pane_height = (display_height > 0) ? display_height : PANE_HEIGHT_DEFAULT_FLUID;
    grid_cols = (pane_width + GRID_CELL_SIZE - 1) / GRID_CELL_SIZE;
    grid_rows = (pane_height + GRID_CELL_SIZE - 1) / GRID_CELL_SIZE;
    if (grid_cols > GRID_COLS_MAX) grid_cols = GRID_COLS_MAX;
    if (grid_rows > GRID_ROWS_MAX) grid_rows = GRID_ROWS_MAX;
    grid_cell_count = grid_cols * grid_rows;

    initializeParticles();
}

void FluidEffect::initializeParticles() {
    num_active_particles = MAX_PARTICLES_FLUID; // Use all particles
    if (!dma_display) return; // Should not happen if setup correctly

    const int panel_width = (pane_width >= 2) ? (pane_width / 2) : pane_width;

    for (int i = 0; i < num_active_particles; ++i) {
        const bool use_right_panel = (panel_width > 0) && (i >= (num_active_particles / 2));
        particles[i].panel_index = use_right_panel ? 1 : 0;
        const int x_min = use_right_panel ? panel_width : 0;
        const int x_max = use_right_panel ? (pane_width - 1) : (panel_width - 1);

        // Initial position: a block of particles in the upper middle part of the screen.
        particles[i].x = random(x_min, x_max + 1);
        particles[i].y = random(pane_height / 4, pane_height / 2);

        // Initial velocity: small random velocities.
        particles[i].xv = ((float)random(-50, 51) / 100.0f) * 0.5f; // Random between -0.25 and +0.25
        particles[i].yv = ((float)random(-50, 51) / 100.0f) * 0.5f; // Random between -0.25 and +0.25
    }
}

void FluidEffect::applyGravityAndAccel() {
    float current_gravity_x = 0.0f;
    float current_gravity_y = GRAVITY_BASE; // Default gravity downwards

    // Check if accelerometer is enabled and available
    if (accelerometer_enabled && *accelerometer_enabled && accelerometer) {
        float accelX = 0.0f;
        float accelY = 0.0f;
        float accelZ = 0.0f;
        constexpr unsigned long EFFECT_ACCEL_SAMPLE_INTERVAL_MS = 15;

        if (getLatestAcceleration(accelX, accelY, accelZ, EFFECT_ACCEL_SAMPLE_INTERVAL_MS)) {
            #if DEBUG_FLUID_EFFECT
            static unsigned long lastAccelLogMs = 0;
            const unsigned long nowMs = millis();
            if (nowMs - lastAccelLogMs >= 200) {
                Serial.print("Fluid accel x=");
                Serial.print(accelX, 3);
                Serial.print(" y=");
                Serial.print(accelY, 3);
                Serial.print(" z=");
                Serial.println(accelZ, 3);
                lastAccelLogMs = nowMs;
            }
            #endif

            // When accel is valid, use it as the primary gravity source.
            current_gravity_x = 0.0f;
            current_gravity_y = 0.0f;

            // Adjust "gravity" based on accelerometer readings.
            // Controller orientation: +Y up, +X away from user, +Z to the right.
            // Map accel Z -> screen X (right), accel Y -> screen Y (down).
            current_gravity_x += accelZ * ACCEL_SENSITIVITY_MULTIPLIER;
            current_gravity_y += accelY * ACCEL_SENSITIVITY_MULTIPLIER;
        }
    }

    for (int i = 0; i < num_active_particles; ++i) {
        // Apply acceleration to velocity (Euler integration).
        particles[i].xv += current_gravity_x * DT_FLUID;
        particles[i].yv += current_gravity_y * DT_FLUID;
    }
}

void FluidEffect::applyParticleInteractions() {
    if (num_active_particles <= 1) return;
    if (grid_cell_count <= 0) {
        for (int i = 0; i < num_active_particles; ++i) {
            for (int j = i + 1; j < num_active_particles; ++j) { // Fallback O(n^2)
                if (particles[i].panel_index != particles[j].panel_index) {
                    continue; // Keep panels independent; no cross-panel interaction.
                }
                float dx = particles[j].x - particles[i].x;
                float dy = particles[j].y - particles[i].y;
                float dist_sq = dx * dx + dy * dy;

                // Check if particles are close enough to interact and not at the exact same spot
                if (dist_sq < PARTICLE_INTERACTION_DISTANCE_SQ && dist_sq > 0.0001f) {
                    float dist = sqrt(dist_sq);
                    
                    // Repulsive force = factor * (overlap_distance / distance).
                    // Force is stronger for more overlap and closer particles.
                    float force_magnitude = PARTICLE_REPEL_FACTOR * (PARTICLE_INTERACTION_DISTANCE - dist) / dist;
                    
                    float force_x_component = dx * force_magnitude;
                    float force_y_component = dy * force_magnitude;

                    // Apply force to both particles (equal and opposite).
                    particles[i].xv -= force_x_component * DT_FLUID;
                    particles[i].yv -= force_y_component * DT_FLUID;
                    particles[j].xv += force_x_component * DT_FLUID;
                    particles[j].yv += force_y_component * DT_FLUID;
                }
            }
        }
        return;
    }

    for (int c = 0; c < grid_cell_count; ++c) {
        grid_head[c] = -1;
    }

    for (int i = 0; i < num_active_particles; ++i) {
        int cx = static_cast<int>(particles[i].x) / GRID_CELL_SIZE;
        int cy = static_cast<int>(particles[i].y) / GRID_CELL_SIZE;
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= grid_cols) cx = grid_cols - 1;
        if (cy >= grid_rows) cy = grid_rows - 1;
        const int cell = cy * grid_cols + cx;
        grid_next[i] = grid_head[cell];
        grid_head[cell] = static_cast<int16_t>(i);
    }

    for (int i = 0; i < num_active_particles; ++i) {
        const int cx = static_cast<int>(particles[i].x) / GRID_CELL_SIZE;
        const int cy = static_cast<int>(particles[i].y) / GRID_CELL_SIZE;

        for (int ny = cy - 1; ny <= cy + 1; ++ny) {
            if (ny < 0 || ny >= grid_rows) continue;
            for (int nx = cx - 1; nx <= cx + 1; ++nx) {
                if (nx < 0 || nx >= grid_cols) continue;
                int j = grid_head[ny * grid_cols + nx];
                while (j != -1) {
                    if (j > i && particles[i].panel_index == particles[j].panel_index) {
                        float dx = particles[j].x - particles[i].x;
                        float dy = particles[j].y - particles[i].y;
                        float dist_sq = dx * dx + dy * dy;

                        if (dist_sq < PARTICLE_INTERACTION_DISTANCE_SQ && dist_sq > 0.0001f) {
                            float dist = sqrt(dist_sq);
                            float force_magnitude = PARTICLE_REPEL_FACTOR * (PARTICLE_INTERACTION_DISTANCE - dist) / dist;

                            float force_x_component = dx * force_magnitude;
                            float force_y_component = dy * force_magnitude;

                            particles[i].xv -= force_x_component * DT_FLUID;
                            particles[i].yv -= force_y_component * DT_FLUID;
                            particles[j].xv += force_x_component * DT_FLUID;
                            particles[j].yv += force_y_component * DT_FLUID;
                        }
                    }
                    j = grid_next[j];
                }
            }
        }
    }
}

void FluidEffect::updateParticlePositions() {
    for (int i = 0; i < num_active_particles; ++i) {
        // Apply drag to damp velocity over time.
        particles[i].xv *= PARTICLE_DRAG;
        particles[i].yv *= PARTICLE_DRAG;

        // Update positions (Euler integration).
        particles[i].x += particles[i].xv * DT_FLUID;
        particles[i].y += particles[i].yv * DT_FLUID;
    }
}

void FluidEffect::applyScreenCollisions() {
    if (!dma_display) return;

    const int panel_width = (pane_width >= 2) ? (pane_width / 2) : pane_width;

    for (int i = 0; i < num_active_particles; ++i) {
        const bool use_right_panel = (panel_width > 0) && (particles[i].panel_index == 1);
        const float x_min = use_right_panel ? static_cast<float>(panel_width) : 0.0f;
        const float x_max = use_right_panel ? static_cast<float>(pane_width - 1) : static_cast<float>(panel_width - 1);

        // Collision with left wall
        if (particles[i].x < x_min) {
            particles[i].x = x_min;
            particles[i].xv *= WALL_DAMPING;
        }
        // Collision with right wall
        else if (particles[i].x > x_max) {
            particles[i].x = x_max; // Ensure it's within bounds
            particles[i].xv *= WALL_DAMPING;
        }

        // Collision with top wall
        if (particles[i].y < 0) {
            particles[i].y = 0;
            particles[i].yv *= WALL_DAMPING;
        }
        // Collision with bottom wall
        else if (particles[i].y >= pane_height) {
            particles[i].y = pane_height - 1.0f; // Ensure it's within bounds
            particles[i].yv *= WALL_DAMPING;
        }
    }
}

void FluidEffect::drawParticles() {
    if (!dma_display) return;

    const int panel_width = (pane_width >= 2) ? (pane_width / 2) : pane_width;

    // dma_display->clearScreen(); // This is typically handled by the main display loop before calling updateAndDraw()

    for (int i = 0; i < num_active_particles; ++i) {
        // Determine particle color based on speed.
        float speed_sq = particles[i].xv * particles[i].xv + particles[i].yv * particles[i].yv;
        
        // Map speed to brightness/color. Max expected speed_sq might be ~2.0f for these parameters.
        float t = speed_sq / MAX_SPEED_SQ;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        const int red_intensity = static_cast<int>(20.0f + t * (255.0f - 20.0f));
        const int green_intensity = static_cast<int>(60.0f + (1.0f - t) * (120.0f - 60.0f));
        const int blue_intensity = static_cast<int>(180.0f + (1.0f - t) * (220.0f - 180.0f));

        uint16_t particle_color = dma_display->color565(red_intensity, green_intensity, blue_intensity);

        // Draw the particle. Ensure coordinates are integers and within bounds before drawing.
        int px = static_cast<int>(particles[i].x);
        int py = static_cast<int>(particles[i].y);

        if (particles[i].panel_index == 1 && panel_width > 0) {
            const int x_min = panel_width;
            const int x_max = pane_width - 1;
            px = x_min + (x_max - (px - x_min)); // Mirror X within the right panel.
        }

        if (px >= 0 && px < pane_width && py >= 0 && py < pane_height) {
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
