#ifndef MIC_CONFIG_H
#define MIC_CONFIG_H

#include "perf_tuning.h"

// SPH0645 I2S configuration
#define MIC_I2S_PORT I2S_NUM_1
#define MIC_SAMPLE_RATE 16000
#define MIC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define MIC_CHANNEL_FORMAT I2S_CHANNEL_FMT_RIGHT_LEFT
#define MIC_COMM_FORMAT I2S_COMM_FORMAT_STAND_I2S
#define MIC_DMA_BUF_COUNT LF_MIC_DMA_BUF_COUNT
#define MIC_DMA_BUF_LEN LF_MIC_DMA_BUF_LEN
#define MIC_READ_TIMEOUT_MS 10
#define MIC_I2S_INTR_FLAGS 0

// Match this to the SPH0645 SEL pin wiring.
// LOW  -> mic drives the left slot (WS low)
// HIGH -> mic drives the right slot (WS high)
#define MIC_SEL_IS_RIGHT_SLOT 0
#define MIC_SLOT_COUNT 2
#define MIC_ACTIVE_SLOT_INDEX (MIC_SEL_IS_RIGHT_SLOT ? 1 : 0)

// SPH0645 outputs 24-bit two's complement audio with 18 valid bits left-aligned
// inside each 32-bit container returned by the ESP32 I2S driver.
#define MIC_SAMPLE_SHIFT 14
#define MIC_POWER_UP_DELAY_MS 10

// Mouth animation output range
#define MIC_MOUTH_OPEN_HOLD_MS 80
#define MIC_MIN_BRIGHTNESS 50
#define MIC_MAX_BRIGHTNESS 255

// Envelope extraction and speech tracking
#define MIC_DC_BLOCK_ALPHA 0.975f
#define MIC_SIGNAL_ATTACK_ALPHA 0.45f
#define MIC_SIGNAL_RELEASE_ALPHA 0.12f

// Adaptive noise floor tracking
#define MIC_NOISE_FLOOR_INIT 500.0f
#define MIC_NOISE_FLOOR_MIN 100.0f
#define MIC_NOISE_FLOOR_MAX 20000.0f
#define MIC_NOISE_FLOOR_GATE_BAND 800.0f
#define MIC_NOISE_FLOOR_RISE_ALPHA 0.01f
#define MIC_NOISE_FLOOR_FALL_ALPHA 0.001f
#define MIC_NOISE_GATE_MARGIN 800.0f

// Adaptive peak reference for speaker normalization
#define MIC_PEAK_REF_INIT 4000.0f
#define MIC_PEAK_REF_MIN 4000.0f
#define MIC_PEAK_REF_MAX 40000.0f
#define MIC_PEAK_REF_ATTACK_ALPHA 0.03f
#define MIC_PEAK_REF_RELEASE_ALPHA 0.001f

// Binary mouth state hysteresis on the normalized envelope
#define MIC_MOUTH_OPEN_THRESHOLD 0.18f
#define MIC_MOUTH_CLOSE_THRESHOLD 0.10f

#ifndef DEBUG_MICROPHONE
#define DEBUG_MICROPHONE 0
#endif

#endif // MIC_CONFIG_H
