#pragma once

void displayLoadingBar();
void drawTransFlag();
void drawLGBTFlag();
void staticColor();
void patternPlasma();
void displaySleepMode();
void updateStarfield();
void updateDVDLogos();
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
void createEffectRenderers(bool videoStorageReady);
void initializeEffectAnimations();
void resetEffectView(int view);
