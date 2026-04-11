#pragma once

#ifndef DEBUG_MODE
#define DEBUG_MODE 0          // Set to 1 to enable debug outputs
#endif
#ifndef DEBUG_MICROPHONE
#define DEBUG_MICROPHONE 1    // Set to 1 to enable microphone debug outputs
#endif
#ifndef DEBUG_ACCELEROMETER
#define DEBUG_ACCELEROMETER 0 // Set to 1 to enable accelerometer debug outputs
#endif
#ifndef DEBUG_BRIGHTNESS
#define DEBUG_BRIGHTNESS 0    // Set to 1 to enable brightness debug outputs
#endif
#ifndef DEBUG_VIEWS
#define DEBUG_VIEWS 0         // Set to 1 to enable views debug outputs
#endif
#ifndef DEBUG_VIEW_TIMING
#define DEBUG_VIEW_TIMING 0   // Set to 1 to enable view timing debug outputs
#endif
#ifndef DEBUG_FPS_COUNTER
#define DEBUG_FPS_COUNTER 0   // Set to 1 to enable FPS counter debug outputs
#endif
#ifndef DEBUG_PROXIMITY
#define DEBUG_PROXIMITY 0     // Set to 1 to enable proximity sensor debug logs
#endif
#ifndef TEXT_DEBUG
#define TEXT_DEBUG 0          // Set to 1 to enable text debug outputs
#endif
#ifndef DEBUG_FLUID_EFFECT
#define DEBUG_FLUID_EFFECT 0  // Set to 1 to enable fluid effect debug outputs
#endif
#ifndef DEBUG_VIDEO_PLAYER
#define DEBUG_VIDEO_PLAYER 0  // Set to 1 to enable video player debug logs and overlay
#endif

#if DEBUG_MODE
#define DEBUG_BLE
#endif

#if DEBUG_PROXIMITY
#define LOG_PROX(...) Serial.printf(__VA_ARGS__)
#define LOG_PROX_LN(msg) Serial.println(msg)
#else
#define LOG_PROX(...) \
  do                  \
  {                   \
  } while (0)
#define LOG_PROX_LN(msg) \
  do                     \
  {                      \
  } while (0)
#endif
