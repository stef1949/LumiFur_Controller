#include "rendering/RenderSupport.h"
#include "rendering/EffectRenderer.h"
#include "rendering/FaceRenderer.h"
#include "rendering/DisplayOverlays.h"
#include <FFat.h>
#include "app/RuntimeTuning.h"
#include "assets/bitmaps.h"
#include <Adafruit_PixelDust.h>
#include <array>

namespace
{
constexpr std::size_t color_num = 5;
using colour_arr_t = std::array<uint16_t, color_num>;

uint16_t myDARK, myWHITE, myRED, myGREEN, myBLUE;
colour_arr_t colours;

struct Square
{
  float xpos, ypos;
  float velocityx;
  float velocityy;
  boolean xdir, ydir;
  uint16_t square_size;
  uint16_t colour;
};

const int numSquares = 25;
Square Squares[numSquares];

struct Star
{
  int x;
  int y;
  int speed;
};
const int NUM_STARS = 50;
Star stars[NUM_STARS];

struct DVDLogo
{
  int x, y;
  int vx, vy;
  uint16_t color;
};

DVDLogo logos[2];

const int dvdWidth = 23; // Width of the DVD logo
const int dvdHeight = 10;

unsigned long lastDvdUpdate = 0;
const unsigned long dvdUpdateInterval = runtime::kDvdFrameIntervalMs;


FluidEffect *fluidEffectInstance = nullptr; // Global pointer for our fluid effect object
MonoVideoPlayer *videoPlayerInstance = nullptr;

// Full Screen Spinning Spiral Effect Variables (NEW)
float fullScreenSpiralAngle = 0.0f;
uint8_t fullScreenSpiralColorOffset = 0;
unsigned long lastFullScreenSpiralUpdateTime = 0;
// const unsigned long FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS = 50; // Adjusted for performance
const unsigned long FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS = 0; // Further adjusted if needed

const float FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE = 0.2f;
const uint8_t FULL_SPIRAL_COLOR_SCROLL_SPEED = 2.5;

// Logarithmic Spiral Coefficients (NEW/MODIFIED)
const float LOG_SPIRAL_A_COEFF = 4.0f;  // Initial radius factor (when theta is small or exp(0))
                                        // Smaller 'a' means it starts smaller/tighter from center.
const float LOG_SPIRAL_B_COEFF = 0.10f; // Controls how tightly wound the spiral is.
                                        // Smaller 'b' makes it wind more slowly (appearing tighter initially for a given theta range).
                                        // Larger 'b' makes it expand much faster.
                                        // Positive 'b' for outward spiral, negative for inward.

const float SPIRAL_ARM_COLOR_FACTOR = 5.0f; // Could be based on 'r' now instead of 'theta_arm'
const float SPIRAL_THICKNESS_RADIUS = 1.0f; // Start with 0 for performance, then increase

#define MAX_FPS 45 // Maximum redraw rate, frames/second

#ifndef PDUST_N_COLORS // Allow overriding from a global config if needed
#define PDUST_N_COLORS 8
#endif

#define N_COLORS PDUST_N_COLORS // Number of distinct colors for sand
#define BOX_HEIGHT 4            // Initial height of each color block of sand

// Calculate total number of sand grains.
// Logic: Each color forms a block. There are N_COLORS blocks.
// Width of each block = WIDTH / N_COLORS
// Number of grains per block = (WIDTH / N_COLORS) * BOX_HEIGHT
// Total grains = N_COLORS * (WIDTH / N_COLORS) * BOX_HEIGHT = WIDTH * BOX_HEIGHT
// Original calculation: (BOX_HEIGHT * N_COLORS * 8). If WIDTH=64, N_COLORS=8, BOX_HEIGHT=8,
// this is (8 * 8 * 8) = 512. And WIDTH * BOX_HEIGHT = 64 * 8 = 512. So they are equivalent for these values.
#define N_GRAINS (PANE_WIDTH * BOX_HEIGHT) // Total number of sand grains
// #define N_GRAINS 10
// #include "pixelDustEffect.h" // Include the header for this effect
// #include "main.h"            // Include your main project header if it exists (e.g., for global configs)

// Define global variables declared as extern in the header
uint16_t colors[N_COLORS];
Adafruit_PixelDust sand(PANE_WIDTH, PANE_HEIGHT, N_GRAINS, 1.0f, 128, false); // elasticity 1.0, accel_scale 128
uint32_t prevTime = 0;
static bool pixelDustInitialized = false;

enum SpiralColorMode
{
  SPIRAL_COLOR_PALETTE,
  SPIRAL_COLOR_WHITE
};



} // namespace

void displayLoadingBar();
void drawTransFlag();
void drawLGBTFlag();
void staticColor();
void patternPlasma();
void displaySleepMode();
void initStarfield();
void updateStarfield();
void initDVDLogoAnimation();
void setupPixelDust();
void updateAndDrawFullScreenSpiral(SpiralColorMode colorMode);
void drawDVDLogo(int x, int y, uint16_t color);
void updateDVDLogos();
void PixelDustEffect();
void renderDebugSquaresView();
void renderLoadingBarView();
void renderBsodView();
void renderFlameEffectView();
void renderFluidEffectView();
void renderPixelDustView();
void renderMatrixRainView();
void renderFullscreenSpiralPalette();
void renderFullscreenSpiralWhite();
void renderVideoPlayerView();

void displayLoadingBar()
{
  // dma_display->clearScreen();

  // Draw the loading bar background
  int barWidth = dma_display->width() - 80;               // Width of the loading bar
  int barHeight = 5;                                      // Height of the loading bar
  int barX = (dma_display->width() / 4) - (barWidth / 2); // Center X position
  int barY = (dma_display->height() - barHeight) / 2;     // Center vertically
  // int barRadius = 4;                // Rounded corners
  dma_display->drawRect(barX, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));
  dma_display->drawRect(barX + 64, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));

  // Draw the loading progress
  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  dma_display->fillRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
  dma_display->fillRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
}

void drawTransFlag()
{
  // dma_display->clearScreen(); // Clear the display

  int stripeHeight = dma_display->height() / 5; // Height of each stripe

  // Define colors in RGB565 format with intended values:
  // lightBlue: (0,51,102), pink: (255,10,73), white: (255,255,255)
  uint16_t lightBlue = dma_display->color565(0, (102 / 2), (204 / 2));
  uint16_t pink = dma_display->color565(255, (20 / 2), (147 / 2));
  uint16_t white = dma_display->color565(255, 255, 255);

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, lightBlue);                // Top light blue stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, pink);          // Pink stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, white);     // Middle white stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, pink);      // Pink stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, lightBlue); // Bottom light blue stripe
}

void drawLGBTFlag()
{
  int stripeHeight = dma_display->height() / 6; // Height of each stripe

  // Define colors in RGB565 format with intended values:
  // red: (255,0,0), orange: (255,127,0), yellow: (255,255,0), green: (0,255,0), blue: (0,0,255), purple: (139,0,255)
  uint16_t red = dma_display->color565(255, 0, 0);
  uint16_t orange = dma_display->color565(255, 127, 0);
  uint16_t yellow = dma_display->color565(255, 255, 0);
  uint16_t green = dma_display->color565(0, 255, 0);
  uint16_t blue = dma_display->color565(0, 0, 255);
  uint16_t purple = dma_display->color565(139, 0, 255);

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, red);                   // Top red stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, orange);     // Orange stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, yellow); // Yellow stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, green);  // Green stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, blue);   // Blue stripe
  dma_display->fillRect(0, stripeHeight * 5, dma_display->width(), stripeHeight, purple); // Bottom purple stripe
}

void staticColor()
{
  ensureStaticColorLoaded();
  const CRGB color = getStaticColorCached();
  const uint16_t encodedColor = dma_display->color565(color.r, color.g, color.b);
  dma_display->fillScreen(encodedColor);
}

void patternPlasma()
{
  // Advance plasma animation timing + palette cycling reused by all plasma-based views
  updatePlasmaFace();

  static uint16_t paletteLUT[256];
  static CRGBPalette16 cachedPalette;
  static uint16_t cachedBrightnessScale = 0;
  static bool paletteLUTValid = false;

  if (!paletteLUTValid || cachedBrightnessScale != globalBrightnessScaleFixed || std::memcmp(&cachedPalette, &currentPalette, sizeof(CRGBPalette16)) != 0)
  {
    const uint16_t scale = globalBrightnessScaleFixed;
    for (int i = 0; i < 256; ++i)
    {
      CRGB color = ColorFromPalette(currentPalette, static_cast<uint8_t>(i));
      color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 128) >> 8);
      color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 128) >> 8);
      color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 128) >> 8);
      paletteLUT[i] = dma_display->color565(color.r, color.g, color.b);
    }
    cachedPalette = currentPalette;
    cachedBrightnessScale = scale;
    paletteLUTValid = true;
  }

  // Pre-calculate values that only depend on time_counter
  // These are constant for the entire frame draw
  const uint8_t wibble = sin8(time_counter);
  // Pre-calculate the cosine term and the division by 8 (as shift)
  // Note: cos8 argument is uint8_t, so -time_counter wraps around correctly.
  const uint8_t cos8_val_shifted = cos8(static_cast<uint8_t>(-time_counter)) >> 3; // Calculate cos8(-t)/8 once
  const uint16_t time_val = time_counter;                                          // Use a local copy for calculations
  const uint16_t term2_factor = 128 - wibble;                                      // Calculate 128 - sin8(t) once

  // Get display dimensions once
  const int display_width = dma_display->width();
  const int display_height = dma_display->height();
  int16_t columnWave[PANE_WIDTH];
  int16_t rowWave[PANE_HEIGHT];

  for (int x = 0; x < display_width; ++x)
  {
    columnWave[x] = sin16(x * wibble * 3 + time_val);
  }

  for (int y = 0; y < display_height; ++y)
  {
    rowWave[y] = cos16(y * term2_factor + time_val);
  }

  // Outer loop for X
  for (int x = 0; x < display_width; x++)
  {
    // Pre-calculate terms dependent only on X and time
    const int16_t term1_wave = columnWave[x];
    // The y*x part needs to be inside the Y loop, but we can precalculate x * cos8_val_shifted
    const uint16_t term3_x_factor = x * cos8_val_shifted;
    uint16_t term3Arg = 0;

    // Inner loop for Y
    for (int y = 0; y < display_height; y++)
    {
      // Calculate plasma value 'v'
      // Start with base offset
      int16_t v = 128;
      // Add terms using pre-calculated values where possible
      v += term1_wave;                         // sin16(x*wibble*3 + t)
      v += rowWave[y];                         // cos16(y*(128-wibble) + t)
      v += sin16(term3Arg);                    // sin16(y * (x * cos8(-t) >> 3))

      const uint16_t color565 = paletteLUT[static_cast<uint8_t>(v >> 8)];
      dma_display->drawPixel(x, y, color565);
      term3Arg = static_cast<uint16_t>(term3Arg + term3_x_factor);
    }
  }
}

void displaySleepMode()
{
  static unsigned long lastBlinkTime = 0;
  static bool eyesOpen = false;
  static float animationPhase = 0;
  static unsigned long lastAnimTime = 0;

  // Apply brightness adjustment for breathing effect
  static unsigned long lastBreathTime = 0;
  static float brightness = 0;
  static float breathingDirection = 1; // 1 for increasing, -1 for decreasing

  // Update breathing effect
  if (millis() - lastBreathTime >= 50)
  {
    lastBreathTime = millis();

    // Update breathing brightness
    brightness += breathingDirection * 0.01; // Slow breathing effect

    // Reverse direction at limits
    if (brightness >= 1.0)
    {
      brightness = 1.0;
      breathingDirection = -1;
    }
    else if (brightness <= 0.1)
    { // Keep a minimum brightness
      brightness = 0.1;
      breathingDirection = 1;
    }

    // Apply breathing effect to overall brightness
    uint8_t currentBrightness = sleepBrightness * brightness;
    updateGlobalBrightnessScale(currentBrightness);
  }

  dma_display->clearScreen();

  // Simple animation for floating Zs
  if (millis() - lastAnimTime > 100)
  {
    animationPhase += 0.1;
    lastAnimTime = millis();
  }

  // Draw animated ZZZs with offset based on animation phase
  int offset1 = sin8(animationPhase * 20) / 16;
  int offset2 = sin8((animationPhase + 0.3) * 20) / 16;
  int offset3 = sin8((animationPhase + 0.6) * 20) / 16;

  drawXbm565(22, 2 + offset1, 8, 8, sleepZ1, dma_display->color565(150, 150, 150));
  drawXbm565(32, 0 + offset2, 8, 8, sleepZ2, dma_display->color565(100, 100, 100));
  drawXbm565(42, 1 + offset3, 8, 8, sleepZ3, dma_display->color565(50, 50, 50));

  drawXbm565(76, 2 + offset1, 8, 8, sleepZ1, dma_display->color565(50, 50, 50));
  drawXbm565(86, 0 + offset2, 8, 8, sleepZ2, dma_display->color565(100, 100, 100));
  drawXbm565(96, 1 + offset3, 8, 8, sleepZ3, dma_display->color565(150, 150, 150));

  /*
  // Draw closed or slightly open eyes
  if (millis() - lastBlinkTime > 4000)
  { // Occasional slow blink
    eyesOpen = !eyesOpen;
    lastBlinkTime = millis();
  }
*/
  // Draw nose
  drawXbm565(56, 14, 8, 8, nose, dma_display->color565(100, 100, 100));
  drawXbm565(64, 14, 8, 8, noseL, dma_display->color565(100, 100, 100));

  if (eyesOpen)
  {
    // Draw slightly open eyes - just a small slit
    // dma_display->fillRect(5, 5, 20, 2, dma_display->color565(150, 150, 150));
    // dma_display->fillRect(103, 5, 20, 2, dma_display->color565(150, 150, 150));
  }
  else
  {
    // Draw closed eyes
    // dma_display->drawLine(5, 10, 40, 12, dma_display->color565(150, 150, 150));
    // dma_display->drawLine(83, 10, 40, 12, dma_display->color565(150, 150, 150));
    // dma_display->drawLine(88, 12, 123, 10, dma_display->color565(150, 150, 150));
  }
  // Draw sleeping mouth (slight curve)
  /*
  drawXbm565(0, 20, 10, 8, maw2Closed, dma_display->color565(120, 120, 120));
  drawXbm565(64, 20, 10, 8, maw2ClosedL, dma_display->color565(120, 120, 120));
  */
  // dma_display->drawFastHLine(PANE_WIDTH / 2 - 15, PANE_HEIGHT - 8, 30, dma_display->color565(120, 120, 120));
  drawBluetoothStatusIcon();
  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  if (pairingSnapshot.pairing && pairingSnapshot.passkeyValid)
  {
    drawPairingPasskeyOverlay(pairingSnapshot.passkey);
  }
  presentDisplayFrame();
}

void initStarfield()
{
  for (int i = 0; i < NUM_STARS; i++)
  {
    stars[i].x = random(0, dma_display->width());
    stars[i].y = random(0, dma_display->height());
    stars[i].speed = random(1, 4); // Stars move at different speeds
  }
}

void updateStarfield()
{
  // Update and draw each star
  for (int i = 0; i < NUM_STARS; i++)
  {
    dma_display->drawPixel(stars[i].x, stars[i].y, dma_display->color565(255, 255, 255));
    // Move star leftwards based on its speed
    stars[i].x -= stars[i].speed;
    // Reset star to right edge if it goes off the left side
    if (stars[i].x < 0)
    {
      stars[i].x = dma_display->width() - 1;
      stars[i].y = random(0, dma_display->height());
      stars[i].speed = random(1, 4);
    }
  }
  // presentDisplayFrame();
}

void initDVDLogoAnimation()
{
  logos[0] = {0, 0, 1, 1, dma_display->color565(255, 0, 255)};   // Left panel
  logos[1] = {64, 0, -1, 1, dma_display->color565(0, 255, 255)}; // Right panel
}

void setupPixelDust()
{
  if (!dma_display)
  {
    Serial.println("PixelDust Error: dma_display is not initialized before setupPixelDust() call!");
    return;
  }
  if (!pixelDustInitialized)
  {
    if (!sand.begin())
    {
      Serial.println("PixelDust Error: sand.begin() failed!");
      return;
    }
    pixelDustInitialized = true;
  }
  if (N_COLORS <= 0)
  {
    Serial.println("PixelDust Error: N_COLORS must be greater than zero.");
    return;
  }

  sand.clear();
  sand.randomize();

  static const uint16_t defaultPalette[] = {
      dma_display->color565(64, 64, 64),
      dma_display->color565(120, 79, 23),
      dma_display->color565(228, 3, 3),
      dma_display->color565(255, 140, 0),
      dma_display->color565(255, 237, 0),
      dma_display->color565(0, 128, 38),
      dma_display->color565(0, 77, 255),
      dma_display->color565(117, 7, 135)};

  const size_t paletteCount = sizeof(defaultPalette) / sizeof(defaultPalette[0]);
  // If N_COLORS exceeds the size of defaultPalette, random colors will be used for the extra entries.
  for (int i = 0; i < N_COLORS; ++i)
  {
    // Use default palette color if available, otherwise generate a random color
    uint16_t color;
    if (i < static_cast<int>(paletteCount))
    {
      color = defaultPalette[i];
    }
    else
    {
      // Use a color wheel to ensure distinct fallback colors
      uint8_t hue = (i * 256 / N_COLORS) % 256;
      CRGB rgb;
      hsv2rgb_rainbow(CHSV(hue, 255, 255), rgb);
      color = dma_display->color565(rgb.r, rgb.g, rgb.b);
    }
    colors[i] = color;
  }
}

void updateAndDrawFullScreenSpiral(SpiralColorMode colorMode)
{ // Added colorMode parameter
  unsigned long currentTime = millis();
  if (currentTime - lastFullScreenSpiralUpdateTime < FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS)
  {
    return;
  }
  lastFullScreenSpiralUpdateTime = currentTime;

  // Update animation parameters (rotation, color scroll - color scroll only if palette mode)
  fullScreenSpiralAngle += FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE;
  if (fullScreenSpiralAngle >= TWO_PI)
    fullScreenSpiralAngle -= TWO_PI;

  if (colorMode == SPIRAL_COLOR_PALETTE)
  { // Only update color offset if using palette
    fullScreenSpiralColorOffset += FULL_SPIRAL_COLOR_SCROLL_SPEED;
  }

  const int centerX = PANE_WIDTH / 2;
  const int centerY = PANE_HEIGHT / 2;
  const float maxRadiusSquared = powf(hypotf(PANE_WIDTH / 2.0f, PANE_HEIGHT / 2.0f) * 1.1f, 2.0f);
  const float deltaTheta = 0.05f;
  const float growthFactor = expf(LOG_SPIRAL_B_COEFF * deltaTheta);
  const float rotCos = cosf(deltaTheta);
  const float rotSin = sinf(deltaTheta);

  float radius = LOG_SPIRAL_A_COEFF;
  float sinAngle = sinf(fullScreenSpiralAngle);
  float cosAngle = cosf(fullScreenSpiralAngle);

  float x = radius * cosAngle;
  float y = radius * sinAngle;

  float colorPhase = fullScreenSpiralColorOffset;
  const float colorPhaseStep = deltaTheta * SPIRAL_ARM_COLOR_FACTOR;

  const int maxSteps = 512;
  for (int step = 0; step < maxSteps; ++step)
  {
    const int drawX = static_cast<int>(centerX + x + 0.5f);
    const int drawY = static_cast<int>(centerY + y + 0.5f);

    uint16_t pixel_color;
    if (colorMode == SPIRAL_COLOR_WHITE)
    {
      pixel_color = dma_display->color565(255, 255, 255);
    }
    else
    {
      const uint8_t color_index = static_cast<uint8_t>(colorPhase);
      CRGB crgb_color = ColorFromPalette(RainbowColors_p, color_index, 255, LINEARBLEND);
      pixel_color = dma_display->color565(crgb_color.r, crgb_color.g, crgb_color.b);
    }

    if (SPIRAL_THICKNESS_RADIUS == 0)
    {
      if (drawX >= 0 && drawX < PANE_WIDTH && drawY >= 0 && drawY < PANE_HEIGHT)
      {
        dma_display->drawPixel(drawX, drawY, pixel_color);
      }
    }
    else
    {
      const int side_length = SPIRAL_THICKNESS_RADIUS * 2 + 1;
      const int start_x = drawX - SPIRAL_THICKNESS_RADIUS;
      const int start_y = drawY - SPIRAL_THICKNESS_RADIUS;
      dma_display->fillRect(start_x, start_y, side_length, side_length, pixel_color);
    }

    colorPhase += colorPhaseStep;

    const float rotatedX = x * rotCos - y * rotSin;
    const float rotatedY = x * rotSin + y * rotCos;
    x = rotatedX * growthFactor;
    y = rotatedY * growthFactor;

    if ((x * x + y * y) > maxRadiusSquared)
    {
      break;
    }
  }
}

void drawDVDLogo(int x, int y, uint16_t color)
{
  struct DvdLogoSpan
  {
    uint8_t x;
    uint8_t length;
  };
  struct DvdLogoRow
  {
    uint8_t count = 0;
    DvdLogoSpan spans[(dvdWidth + 1) / 2] = {};
  };

  static bool spansInitialized = false;
  static DvdLogoRow rows[dvdHeight];

  if (!spansInitialized)
  {
    const int byteWidth = (dvdWidth + 7) >> 3;
    for (int row = 0; row < dvdHeight; ++row)
    {
      DvdLogoRow &rowData = rows[row];
      bool runActive = false;
      uint8_t runStart = 0;
      uint8_t runLength = 0;

      for (int pixel = 0; pixel < dvdWidth; ++pixel)
      {
        const int byteIndex = row * byteWidth + (pixel >> 3);
        const uint8_t rowByte = pgm_read_byte(&DvDLogo[byteIndex]);
        const bool pixelSet = (rowByte & static_cast<uint8_t>(0x80U >> (pixel & 7))) != 0;

        if (pixelSet)
        {
          if (!runActive)
          {
            runActive = true;
            runStart = static_cast<uint8_t>(pixel);
            runLength = 1;
          }
          else
          {
            ++runLength;
          }
        }
        else if (runActive)
        {
          DvdLogoSpan &span = rowData.spans[rowData.count++];
          span.x = runStart;
          span.length = runLength;
          runActive = false;
        }
      }

      if (runActive)
      {
        DvdLogoSpan &span = rowData.spans[rowData.count++];
        span.x = runStart;
        span.length = runLength;
      }
    }

    spansInitialized = true;
  }

  for (int row = 0; row < dvdHeight; ++row)
  {
    const DvdLogoRow &rowData = rows[row];
    for (uint8_t spanIndex = 0; spanIndex < rowData.count; ++spanIndex)
    {
      const DvdLogoSpan &span = rowData.spans[spanIndex];
      dma_display->drawFastHLine(x + span.x, y + row, span.length, color);
    }
  }
}

void updateDVDLogos()
{
  const unsigned long now = millis();
  const bool shouldAdvance = (now - lastDvdUpdate) >= dvdUpdateInterval;
  if (shouldAdvance)
  {
    lastDvdUpdate = now;
  }

  for (int i = 0; i < 2; i++)
  {
    DVDLogo &logo = logos[i];
    int minX = (i == 0) ? 0 : 64;
    int maxX = minX + 64;

    if (shouldAdvance)
    {
      logo.x += logo.vx;
      logo.y += logo.vy;

      bool bounced = false;

      if (logo.x < minX)
      {
        logo.x = minX;
        logo.vx = -logo.vx;
        bounced = true;
      }
      else if (logo.x + dvdWidth > maxX)
      {
        logo.x = maxX - dvdWidth;
        logo.vx = -logo.vx;
        bounced = true;
      }

      if (logo.y < 0)
      {
        logo.y = 0;
        logo.vy = -logo.vy;
        bounced = true;
      }
      else if (logo.y + dvdHeight > dma_display->height())
      {
        logo.y = dma_display->height() - dvdHeight;
        logo.vy = -logo.vy;
        bounced = true;
      }

      if (bounced)
      {
        logo.color = dma_display->color565(random(256), random(256), random(256));
      }
    }

    drawDVDLogo(logo.x, logo.y, logo.color);
  }
  // presentDisplayFrame(); // Flip the buffer to show the updated logos
  // Rendering cadence is handled by displayTask; avoid delaying here.
}

void PixelDustEffect()
{
  static uint32_t lastFrameMicros = 0;

  const uint32_t now = micros();
  if ((now - lastFrameMicros) < (1000000L / MAX_FPS))
  {
    return;
  }
  lastFrameMicros = now;

  if (!pixelDustInitialized)
  {
    setupPixelDust();
    if (!pixelDustInitialized)
    {
      return;
    }
  }

  if (!dma_display)
  {
    return;
  }

  int16_t ax_scaled = 0;
  int16_t ay_scaled = static_cast<int16_t>(0.1f * 128.0f);

  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;

  if (getLatestAcceleration(accelX, accelY, accelZ, MOTION_SAMPLE_INTERVAL_FAST))
  {
    ax_scaled = static_cast<int16_t>(accelX * 1000.0f);
    ay_scaled = static_cast<int16_t>(accelY * 1000.0f);
  }

  sand.iterate(ax_scaled, ay_scaled, 0);

  dimension_t px, py;
  for (grain_count_t i = 0; i < N_GRAINS; ++i)
  {
    sand.getPosition(i, &px, &py);
    int color_index = 0;
    if (N_COLORS > 0)
    {
      color_index = static_cast<int>((static_cast<uint32_t>(px) * N_COLORS) / PANE_WIDTH);
      if (color_index >= N_COLORS)
      {
        color_index = N_COLORS - 1;
      }
    }
    const int px_i = static_cast<int>(px);
    const int py_i = static_cast<int>(py);
    if (px_i >= 0 && py_i >= 0 && px_i < PANE_WIDTH && py_i < PANE_HEIGHT)
    {
      dma_display->drawPixel(px_i, py_i, colors[color_index]);
    }
  }
}

void renderDebugSquaresView()
{
  for (int i = 0; i < numSquares; i++)
  {
    dma_display->fillRect(Squares[i].xpos, Squares[i].ypos, Squares[i].square_size, Squares[i].square_size, Squares[i].colour);

    if (Squares[i].square_size + Squares[i].xpos >= dma_display->width())
    {
      Squares[i].velocityx *= -1;
    }
    else if (Squares[i].xpos <= 0)
    {
      Squares[i].velocityx = abs(Squares[i].velocityx);
    }

    if (Squares[i].square_size + Squares[i].ypos >= dma_display->height())
    {
      Squares[i].velocityy *= -1;
    }
    else if (Squares[i].ypos <= 0)
    {
      Squares[i].velocityy = abs(Squares[i].velocityy);
    }

    Squares[i].xpos += Squares[i].velocityx;
    Squares[i].ypos += Squares[i].velocityy;
  }
}

void renderLoadingBarView()
{
  drawXbm565(23, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));
  drawXbm565(88, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));

  displayLoadingBar();
  loadingProgress++;
  if (loadingProgress > loadingMax)
  {
    loadingProgress = 0;
  }
}

void renderBsodView()
{
  dma_display->fillScreen(dma_display->color565(0, 0, 255));
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setTextSize(2);
  dma_display->setCursor(5, 15);
  dma_display->print(":(");
  dma_display->setCursor(69, 15);
  dma_display->print(":(");
}

void renderFlameEffectView()
{
  updateAndDrawFlameEffect();
}

void renderFluidEffectView()
{
  if (fluidEffectInstance)
  {
    fluidEffectInstance->updateAndDraw();
  }
  else
  {
    dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), 0);
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->setTextColor(dma_display->color565(255, 0, 0));
    dma_display->setCursor(5, 15);
    dma_display->print("Fluid Err");
  }
}

void renderPixelDustView()
{
  static bool logged = false;
  if (!logged)
  {
    LOG_DEBUG_LN("PixelDust view active");
    logged = true;
  }
  PixelDustEffect();
}

void renderMatrixRainView() {
  updateAndDrawMatrixRainEffect();
}

void renderFullscreenSpiralPalette()
{
  updateAndDrawFullScreenSpiral(SPIRAL_COLOR_PALETTE);
}

void renderFullscreenSpiralWhite()
{
  updateAndDrawFullScreenSpiral(SPIRAL_COLOR_WHITE);
}

void renderVideoPlayerView()
{
  if (videoPlayerInstance)
  {
    videoPlayerInstance->updateAndDraw(dma_display, micros());
#if DEBUG_VIDEO_PLAYER
    static unsigned long lastVideoDebugLogMs = 0;
    const MonoVideoDebugInfo info = videoPlayerInstance->debugInfo();
    const unsigned long displayedFrame = (info.frameCount > 0U) ? (info.currentFrameIndex + 1U) : 0U;
    const unsigned long nowMs = millis();
    if ((nowMs - lastVideoDebugLogMs) >= 1000UL)
    {
      Serial.printf(
          "Video dbg ready=%u open=%u frame=%lu/%lu interval=%luus rew=%lu fail=%lu catch=%u max=%u decode=%luus offset=%lu err=%s\n",
          static_cast<unsigned int>(videoPlayerInstance->ready()),
          static_cast<unsigned int>(info.fileOpen),
          displayedFrame,
          static_cast<unsigned long>(info.frameCount),
          static_cast<unsigned long>(info.frameIntervalMicros),
          static_cast<unsigned long>(info.rewindCount),
          static_cast<unsigned long>(info.decodeFailureCount),
          static_cast<unsigned int>(info.lastCatchupFrames),
          static_cast<unsigned int>(info.maxCatchupFrames),
          static_cast<unsigned long>(info.lastDecodeMicros),
          static_cast<unsigned long>(info.currentFileOffset),
          videoPlayerInstance->error());
      lastVideoDebugLogMs = nowMs;
    }

    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->setTextColor(dma_display->color565(255, 255, 0));

    char line1[24];
    char line2[24];
    char line3[24];
    snprintf(line1,
             sizeof(line1),
             "fr %lu/%lu %lums",
             displayedFrame,
             static_cast<unsigned long>(info.frameCount),
             static_cast<unsigned long>((info.frameIntervalMicros + 500U) / 1000U));
    snprintf(line2,
             sizeof(line2),
             "rw %lu de %lu c%u",
             static_cast<unsigned long>(info.rewindCount),
             static_cast<unsigned long>(info.decodeFailureCount),
             static_cast<unsigned int>(info.lastCatchupFrames));
    snprintf(line3,
             sizeof(line3),
             "dc %luus of %lu",
             static_cast<unsigned long>(info.lastDecodeMicros),
             static_cast<unsigned long>(info.currentFileOffset));

    dma_display->setCursor(1, 9);
    dma_display->print(line1);
    dma_display->setCursor(1, 17);
    dma_display->print(line2);
    dma_display->setCursor(1, 25);
    dma_display->print(line3);
#endif
    return;
  }

  dma_display->setFont(&TomThumb);
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 0, 0));
  dma_display->setCursor(3, 12);
  dma_display->print("Video Err");
  dma_display->setCursor(3, 22);
  dma_display->print("Init fail");
}

void createEffectRenderers(bool videoStorageReady)
{
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
  // Ensure dma_display, accel, and accelerometerEnabled are initialized before this line
  fluidEffectInstance = new FluidEffect(dma_display, &accel, &accelerometerEnabled);
#else
  // Pass nullptr for accelerometer if not available/compiled
  fluidEffectInstance = new FluidEffect(dma_display, nullptr, &accelerometerEnabled);
#endif

  if (!fluidEffectInstance)
  {
    Serial.println("FATAL: Failed to allocate FluidEffect instance!");
    // Handle error appropriately, e.g., by blinking an LED or halting.
    // For now, it will just print "Fluid Error" in displayCurrentView if null.
  }

  videoPlayerInstance = new MonoVideoPlayer(FFat, "/videos/bad_apple.lfv", PANE_WIDTH, PANE_HEIGHT, PANEL_WIDTH, DEBUG_VIDEO_PLAYER != 0);
  if (!videoPlayerInstance)
  {
    Serial.println("FATAL: Failed to allocate MonoVideoPlayer instance!");
  }
  else if (!videoStorageReady)
  {
    Serial.println("Video player created; waiting for FFat storage to become available.");
  }

  // ... rest of your setup() code ...
  // e.g., currentView = getLastView();

  // If the initial view is the fluid effect, call begin()
  if (currentView == VIEW_FLUID_EFFECT && fluidEffectInstance)
  { // Assuming 16 is the fluid effect view
    fluidEffectInstance->begin();
  }
  if (currentView == VIEW_VIDEO_PLAYER && videoPlayerInstance)
  {
    videoPlayerInstance->begin();
  }


}
void initializeEffectAnimations()
{
  ////////Setup Bouncing Squares////////
  myDARK = dma_display->color565(64, 64, 64);
  myWHITE = dma_display->color565(192, 192, 192);
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);

  colours = {{myDARK, myWHITE, myRED, myGREEN, myBLUE}};

  // Create some random squares
  for (int i = 0; i < numSquares; i++)
  {
    Squares[i].square_size = random(2, 10);
    Squares[i].xpos = random(0, dma_display->width() - Squares[i].square_size);
    Squares[i].ypos = random(0, dma_display->height() - Squares[i].square_size);
    Squares[i].velocityx = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    Squares[i].velocityy = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    int random_num = random(colours.size());
    Squares[i].colour = colours[random_num];
  }

  // Initialize the starfield
  initStarfield();
  initDVDLogoAnimation();


}
void resetEffectView(int view)
{
    if (view == VIEW_FLUID_EFFECT)
    { // If switching to Fluid Animation view
      if (fluidEffectInstance)
      {
        fluidEffectInstance->begin(); // Reset fluid effect state
      }
    }
    if (view == VIEW_FLAME_EFFECT)
    {
      initFlameEffect(dma_display);
    }
    if (view == VIEW_MATRIX_RAIN || view == VIEW_MATRIX_FACE)
    {
      initMatrixRainEffect(dma_display);
    }
    if (view == VIEW_PIXEL_DUST)
    {
      setupPixelDust();
    }
    if (view == VIEW_DINO_GAME)
    {
      resetDinoGame(millis());
    }
    if (view == VIEW_VIDEO_PLAYER && videoPlayerInstance)
    {
      videoPlayerInstance->begin();
    }
}
