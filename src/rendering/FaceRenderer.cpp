#include "rendering/RenderSupport.h"
#include "rendering/FaceRenderer.h"
#include "assets/bitmaps.h"

namespace
{
// Derived from the Alt_ bitmap exports with the source XBM bit order/background normalized.
static const uint8_t AltFaceNose[] PROGMEM = {
    0xb0, 0xf8, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
static const uint8_t AltFaceBlushL[] PROGMEM = {
    0x80, 0x8c, 0x00, 0x9a, 0xc0, 0xb6, 0x80, 0xec, 0x00, 0x80};
static const uint8_t AltFaceBrowL[] PROGMEM = {
    0xc0, 0x80, 0xc0, 0x86, 0x80, 0x8e, 0x00, 0x9e, 0x00, 0xbe, 0x00, 0xfc, 0x00, 0xf0};
static const uint8_t AltFaceScleraL[] PROGMEM = {
    0x7f, 0x00, 0x80, 0xff, 0x01, 0x80, 0xff, 0x0f, 0x80, 0xff, 0x3f, 0x80, 0xff, 0xff, 0x80, 0xfe,
    0xff, 0x82, 0xfc, 0xff, 0x86, 0xfc, 0xff, 0x8e, 0xf8, 0xff, 0x9e, 0xf0, 0xff, 0xfe, 0xe0, 0xff,
    0xbe, 0xc0, 0xff, 0x86, 0x80, 0x3f, 0x80};
static const uint8_t AltFaceBlushR[] PROGMEM = {
    0xd8, 0x80, 0x6c, 0x80, 0xb6, 0x80, 0xdb, 0x80, 0x40, 0x80};
static const uint8_t AltFaceBrowR[] PROGMEM = {
    0xc0, 0x80, 0xf0, 0x80, 0xf8, 0x80, 0x7c, 0x80, 0x3e, 0x80, 0x1f, 0x80, 0x07, 0x80};
static const uint8_t AltFaceScleraR[] PROGMEM = {
    0x00, 0x00, 0xfe, 0x00, 0xc0, 0xfe, 0x00, 0xf8, 0xfe, 0x00, 0xfe, 0xfe, 0x80, 0xff, 0xfe, 0xe0,
    0xff, 0xbe, 0xf0, 0xff, 0x9e, 0xf8, 0xff, 0x9e, 0xfc, 0xff, 0x8e, 0xff, 0xff, 0x86, 0xfe, 0xff,
    0x82, 0xf0, 0xff, 0x80, 0x00, 0xfe, 0x80};
static const uint8_t AltFacePupil[] PROGMEM = {
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0};
} // namespace

namespace {
constexpr uint16_t plasmaSpeed = 2;
CRGBPalette16 palettes[] = {ForestColors_p, LavaColors_p, HeatColors_p, RainbowColors_p, CloudColors_p, PartyColors_p};
}

namespace
{
constexpr int MOUTH_WIDTH = 64;
constexpr int MOUTH_HEIGHT = 22;
constexpr int MOUTH_COLLAPSED_HEIGHT = 13;
constexpr std::size_t MOUTH_BITMAP_BYTES =
    static_cast<std::size_t>((MOUTH_WIDTH + 7) / 8) * MOUTH_HEIGHT;

std::uint8_t gMouthFrameRight[MOUTH_BITMAP_BYTES] = {};
std::uint8_t gMouthFrameLeft[MOUTH_BITMAP_BYTES] = {};
std::uint8_t gCachedMouthFrameIndex = 0xff;
} // namespace


void prepareInterpolatedMouthFrames(std::uint8_t rawOpenness)
{
  const std::uint8_t frameIndex = mouth::quantizeOpenness(rawOpenness);
  if (frameIndex == gCachedMouthFrameIndex)
  {
    return;
  }

  const std::uint8_t frameOpenness = mouth::opennessForFrame(frameIndex);
  const bool rightBuilt = mouth::buildInterpolatedXbm(
      maw2Closed, maw2,
      MOUTH_WIDTH, MOUTH_HEIGHT, MOUTH_COLLAPSED_HEIGHT,
      frameOpenness, false,
      gMouthFrameRight, sizeof(gMouthFrameRight));
  const bool leftBuilt = mouth::buildInterpolatedXbm(
      maw2ClosedL, maw2L,
      MOUTH_WIDTH, MOUTH_HEIGHT, MOUTH_COLLAPSED_HEIGHT,
      frameOpenness, true,
      gMouthFrameLeft, sizeof(gMouthFrameLeft));

  if (rightBuilt && leftBuilt)
  {
    gCachedMouthFrameIndex = frameIndex;
  }
}

void drawInterpolatedMouthPlasma(std::uint8_t rightTimeOffset,
                                 std::uint8_t leftTimeOffset,
                                 float scale,
                                 float animSpeed,
                                 std::uint8_t brightness)
{
  prepareInterpolatedMouthFrames(micGetMouthOpenness());
  const bool useOverrideCurve = mouthMicBrightnessOverrideActive();
  const std::uint8_t effectiveBrightness = useOverrideCurve
                                               ? micComputeMouthOverrideBrightness(
                                                     brightness,
                                                     requestedPanelBrightness())
                                               : brightness;
  drawPlasmaXbm(0, 10, MOUTH_WIDTH, MOUTH_HEIGHT, gMouthFrameRight,
                rightTimeOffset, scale, animSpeed, effectiveBrightness, useOverrideCurve);
  drawPlasmaXbm(64, 10, MOUTH_WIDTH, MOUTH_HEIGHT, gMouthFrameLeft,
                leftTimeOffset, scale, animSpeed, effectiveBrightness, useOverrideCurve);
}

void drawPlasmaFace()
{
  // Keep idle hover offsets in sync even if the main loop stalls
  updateIdleHoverAnimation();

  // Combine bounce and idle hover offsets
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset_right = idleEyeXOffset; // X offset for the right eye (0,0 based)
  int final_x_offset_left = idleEyeXOffset;  // X offset for the left eye (96,0 based)

  // Draw eyes with different plasma parameters
  int y_offset = currentEyeYOffset;                                                    // Get current bounce offset for eyes
  drawPlasmaXbm(0 + final_x_offset_right, 0 + final_y_offset, 32, 16, Eye, 0, 1.0);    // Right eye
  drawPlasmaXbm(96 + final_x_offset_left, 0 + final_y_offset, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  // Nose with finer pattern
  drawPlasmaXbm(56, 14, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 14, 8, 8, noseL, 64, 2.0);
  // Mouth with larger pattern

  // drawPlasmaXbm(0, 16, 64, 16, maw, 32, 2.0);
  // drawPlasmaXbm(64, 16, 64, 16, mawL, 32, 2.0);

  const uint8_t mouthBrightness = micGetMouthBrightness();
  drawInterpolatedMouthPlasma(32, 32, 0.5f, 0.2f, mouthBrightness);
}

void updatePlasmaFace()
{
  static unsigned long lastUpdate = 0;
  static unsigned long lastPaletteChange = 0;
  const uint16_t frameDelay = 2;               // Delay for plasma animation update
  const unsigned long paletteInterval = 10000; // Change palette every 10 seconds
  unsigned long now = millis();

  if (now - lastUpdate >= frameDelay)
  {
    lastUpdate = now;
    time_counter += plasmaSpeed; // Update plasma animation counter
    facePlasmaDirty = true;
  }

  if (now - lastPaletteChange > paletteInterval)
  {
    lastPaletteChange = now;
    currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
    facePlasmaDirty = true;
  }
}

void updateRotatingSpiral()
{
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  const unsigned long rotationFrameInterval = 11; // ~90 FPS (11ms per frame)
  unsigned long currentTime = millis();

  if (currentTime - lastUpdate < rotationFrameInterval)
    return;
  lastUpdate = currentTime;

  // Calculate color once per frame
  static uint8_t colorIndex = 0;
  colorIndex += 2; // Slower color transition
  CRGB currentColor = ColorFromPalette(currentPalette, colorIndex);
  uint16_t color = dma_display->color565(
      scaleRawFaceComponentForPanelHeadroom(currentColor.r),
      scaleRawFaceComponentForPanelHeadroom(currentColor.g),
      scaleRawFaceComponentForPanelHeadroom(currentColor.b));

  // Optimized rotation with pre-calculated values
  float radians = currentAngle * PI / 180.0;
  float cosA = cos(radians);
  float sinA = sin(radians);
  currentAngle = fmod(currentAngle + 4, 360); // Slower rotation (4° per frame)

  // Clear previous spiral frame
  // dma_display->fillRect(24, 15, 32, 25, 0);
  // dma_display->fillRect(105, 15, 32, 25, 0);
  //  Draw both spirals using pre-transformed coordinates
  drawRotatedBitmap(24, 15, spiral, 25, 25, cosA, sinA, color);
  drawRotatedBitmap(105, 15, spiralL, 25, 25, cosA, sinA, color);
}

void blinkingEyes()
{
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  // --- Eye Configuration ---
  const uint8_t *rightEyeBitmap = (const uint8_t *)slanteyes; // Default
  const uint8_t *leftEyeBitmap = (const uint8_t *)slanteyesL; // Default
  int eyeWidth = 24, eyeHeight = 16;
  int rightEyeX = 2, rightEyeY = 0;
  int leftEyeX = 102, leftEyeY = 0;
  bool usePlasma = true;                                    // Most views use plasma
  uint16_t solidColor = dma_display->color565(0, 255, 255); // Default cyan, not used if usePlasma is true

  // Select eye assets and properties based on the current view
  switch (currentView)
  {
  case VIEW_NORMAL_FACE: // Normal
  case VIEW_BLUSH_FACE:  // Blush
    rightEyeBitmap = (const uint8_t *)Eye;
    leftEyeBitmap = (const uint8_t *)EyeL;
    eyeWidth = 32;
    eyeHeight = 16;
    rightEyeX = 0;
    rightEyeY = 0;
    leftEyeX = 96;
    leftEyeY = 0;
    break;
  case VIEW_SEMICIRCLE_EYES: // Semicircle
    rightEyeBitmap = (const uint8_t *)semicircleeyes;
    leftEyeBitmap = (const uint8_t *)semicircleeyes; // Same bitmap, different plasma offset
    eyeWidth = 32;
    eyeHeight = 12;
    rightEyeX = 2;
    rightEyeY = 2;
    leftEyeX = 94;
    leftEyeY = 2;
    break;
  case VIEW_X_EYES: // X eyes
    rightEyeBitmap = (const uint8_t *)x_eyes;
    leftEyeBitmap = (const uint8_t *)x_eyes; // Same bitmap
    eyeWidth = 31;
    eyeHeight = 15;
    rightEyeX = 0;
    rightEyeY = 0;
    leftEyeX = 96;
    leftEyeY = 0;
    break;
  case VIEW_SLANT_EYES: // Slant eyes (This is also the default)
    // Values are already set by default
    break;
    case VIEW_ANGRY_FACE:
    {
      // Drawn face. Narrowed slash eyes, heavy furrowed brows, short snarl.
      uint16_t eye = dma_display->color565(40, 255, 70);
      if (staticColorModeEnabled)
      {
        ensureStaticColorLoaded();
        eye = dma_display->color565(getStaticColorCached().r, getStaticColorCached().g, getStaticColorCached().b);
      }
      const uint16_t brow = dma_display->color565(255, 40, 0);
      const uint16_t fang = dma_display->color565(230, 255, 230);

      // Right eye: high at the temple, low toward the nose.
      for (int t = 0; t < 28; ++t)
      {
        const int x = 4 + t;
        const int y = 6 + t / 6;
        const int thick = (t < 22) ? 3 : 2;
        for (int k = 0; k < thick; ++k)
        {
          dma_display->drawPixel(x, y + k, eye);
        }
      }
      dma_display->drawPixel(8, 6, fang);
      dma_display->drawPixel(9, 6, fang);

      // Left eye, mirrored.
      for (int t = 0; t < 28; ++t)
      {
        const int x = 123 - t;
        const int y = 6 + t / 6;
        const int thick = (t < 22) ? 3 : 2;
        for (int k = 0; k < thick; ++k)
        {
          dma_display->drawPixel(x, y + k, eye);
        }
      }
      dma_display->drawPixel(118, 6, fang);
      dma_display->drawPixel(119, 6, fang);

      // Furrowed brows, three pixels thick.
      for (int k = 0; k < 3; ++k)
      {
        for (int t = 0; t < 30; ++t)
        {
          dma_display->drawPixel(2 + t, 1 + k + t / 10, brow);
          dma_display->drawPixel(125 - t, 1 + k + t / 10, brow);
        }
      }

      // Snarl and fangs.
      for (int t = 0; t < 14; ++t)
      {
        dma_display->drawPixel(50 + t, 24 - t / 4, brow);
        dma_display->drawPixel(77 - t, 24 - t / 4, brow);
        dma_display->drawPixel(50 + t, 25 - t / 4, brow);
        dma_display->drawPixel(77 - t, 25 - t / 4, brow);
      }
      for (int k = 0; k < 4; ++k)
      {
        dma_display->drawPixel(56, 22 + k, fang);
        dma_display->drawPixel(57, 22 + k, fang);
        dma_display->drawPixel(70, 22 + k, fang);
        dma_display->drawPixel(71, 22 + k, fang);
      }
      return;
    }
  case VIEW_SPIRAL_EYES: // Spiral eyes
    return;              // Spiral view handles its own drawing, so we exit here.
  case VIEW_CIRCLE_EYES: // Circle eyes
    rightEyeBitmap = (const uint8_t *)circleeyes;
    leftEyeBitmap = (const uint8_t *)circleeyes; // Same bitmap
    eyeWidth = 25;
    eyeHeight = 21;
    rightEyeX = 10;
    rightEyeY = 2;
    leftEyeX = 93;
    leftEyeY = 2;
    break;
  case VIEW_ALT_FACE:
  {
    const uint16_t pupilColor = dma_display->color565(0, 0, 0);
    constexpr uint8_t kAltEyeTimeOffsetRight = 80;
    constexpr uint8_t kAltEyeTimeOffsetLeft = 208;

    drawBitmapAdvanced(10 + final_x_offset, 4 + final_y_offset, 23, 13, Alt_scleraR,
                       solidColor, blinkProgress, true, kAltEyeTimeOffsetRight);
    drawBitmapAdvanced(95 + final_x_offset, 4 + final_y_offset, 23, 13, Alt_scleraL,
                       solidColor, blinkProgress, true, kAltEyeTimeOffsetLeft);
    drawBitmapWithBlink(19 + final_x_offset, 5 + final_y_offset, 3, 11, AltFacePupil,
                        pupilColor, blinkProgress);
    drawBitmapWithBlink(106 + final_x_offset, 5 + final_y_offset, 3, 11, AltFacePupil,
                        pupilColor, blinkProgress);
    return;
  }
    // Default case uses the slanteyes defined before the switch
  }

  // Draw the right eye (viewer's perspective)
  drawBitmapAdvanced(rightEyeX + final_x_offset, rightEyeY + final_y_offset,
                     eyeWidth, eyeHeight, rightEyeBitmap, solidColor,
                     blinkProgress, usePlasma, 0);

  // Draw the left eye with a plasma phase offset if plasma is used
  drawBitmapAdvanced(leftEyeX + final_x_offset, leftEyeY + final_y_offset,
                     eyeWidth, eyeHeight, leftEyeBitmap, solidColor,
                     blinkProgress, usePlasma, 128);
}

void drawBlush()
{
// MARK: Debug: Print blush brightness
#if DEBUG_VIEWS
  Serial.print("Blush brightness: ");
#endif
  // Set blush color based on brightness
  const uint8_t scaledBlushBrightness =
      scaleRawFaceComponentForPanelHeadroom(blushBrightness);
  uint16_t blushColor = dma_display->color565(
      scaledBlushBrightness, 0, scaledBlushBrightness);

  const int blushWidth = 11;
  const int blushHeight = 13;
  const int blushCount = 3;
  const int blushSpacing = blushWidth - 4;
  const int blushY = 1;
  const int rightStartX = 33;
  const int leftStartX = 70;
  const int totalBlushWidth = blushWidth + (blushCount - 1) * blushSpacing;

  // MARK: Clear only the blush area to prevent artifacts
  // dma_display->fillRect(rightStartX, blushY, totalBlushWidth, blushHeight, 0);
  // dma_display->fillRect(leftStartX, blushY, totalBlushWidth, blushHeight, 0);

  for (int i = 0; i < blushCount; ++i)
  {
    drawXbm565(rightStartX + (i * blushSpacing), blushY, blushWidth, blushHeight, blush, blushColor);
    drawXbm565(leftStartX + (i * blushSpacing), blushY, blushWidth, blushHeight, blushL, blushColor);
  }
}

void baseFace()
{
  // Update idle hover here so the render task always sees fresh offsets
  updateIdleHoverAnimation();

  // Combine all offsets for final positioning.
  // These will be used for any facial feature that should move with the hover/bounce effect.
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  if (currentView == VIEW_ALT_FACE)
  {
    const uint16_t blushColor = dma_display->color565(
        scaleRawFaceComponentForPanelHeadroom(255),
        scaleRawFaceComponentForPanelHeadroom(110),
        scaleRawFaceComponentForPanelHeadroom(150));
    drawPlasmaXbm(22 + final_x_offset, 0 + final_y_offset, 9, 7, Alt_BrowR, 16, 2.0f);
    drawPlasmaXbm(96 + final_x_offset, 0 + final_y_offset, 9, 7, Alt_BrowL, 144, 2.0f);
    drawXbm565(8 + final_x_offset, 16 + final_y_offset, 9, 5, Alt_BlushR, blushColor);
    drawXbm565(110 + final_x_offset, 16 + final_y_offset, 9, 5, Alt_BlushL, blushColor);
  }

  blinkingEyes(); // This function now correctly uses the global offsets internally

  const uint8_t mouthBrightness = micGetMouthBrightness();
  drawInterpolatedMouthPlasma(0, 128, 1.0f, 0.2f, mouthBrightness);

  if (currentView == VIEW_ALT_FACE)
  {
    drawPlasmaXbm(58 + final_x_offset, 7 + final_y_offset, 4, 2, Alt_Nose, 96, 2.0f);
    drawPlasmaXbm(68 + final_x_offset, 7 + final_y_offset, 4, 2, Alt_Nose, 96, 2.0f);
  }
  else
  {
    drawPlasmaXbm(56, 4 + final_y_offset, 8, 8, nose, 64, 2.0);
    drawPlasmaXbm(64, 4 + final_y_offset, 8, 8, noseL, 64, 2.0);
  }

  facePlasmaDirty = false;
}

void renderFaceWithPlasma()
{
  baseFace();
  updatePlasmaFace();
}

void renderSpiralEyesView()
{
  updatePlasmaFace();
  baseFace();
  updateRotatingSpiral();
}

void renderPlasmaFaceView()
{
  drawPlasmaFace();
  updatePlasmaFace();
}

void renderMatrixFaceView() {
  dma_display->fillScreen(0);

  const int eyeW = 32;
  const int eyeH = 16;
  const int rightEyeX = 0;
  const int rightEyeY = 0;
  const int leftEyeX = 96;
  const int leftEyeY = 0;

  const int noseW = 8;
  const int noseH = 8;
  const int rightNoseX = 56;
  const int leftNoseX = 64;
  const int noseY = 14;

  const int mouthW = 64;
  const int mouthH = 22;
  const int mouthY = 10;

  matrixRainAdvance();

  drawMatrixRainThroughXbm(rightEyeX, rightEyeY, eyeW, eyeH, (const uint8_t *)Eye);
  drawMatrixRainThroughXbm(leftEyeX, leftEyeY, eyeW, eyeH, (const uint8_t *)EyeL);

  drawMatrixRainThroughXbm(rightNoseX, noseY, noseW, noseH, (const uint8_t *)nose);
  drawMatrixRainThroughXbm(leftNoseX, noseY, noseW, noseH, (const uint8_t *)noseL);

  prepareInterpolatedMouthFrames(micGetMouthOpenness());
  drawMatrixRainThroughXbm(0, mouthY, mouthW, mouthH, gMouthFrameRight);
  drawMatrixRainThroughXbm(64, mouthY, mouthW, mouthH, gMouthFrameLeft);
}
