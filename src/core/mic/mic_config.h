#ifndef MIC_CONFIG_H
#define MIC_CONFIG_H

// SPH0645 I2S configuration
#define MIC_I2S_PORT I2S_NUM_1
#define MIC_SAMPLE_RATE 16000
#define MIC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define MIC_CHANNEL_FORMAT I2S_CHANNEL_FMT_ONLY_LEFT
#define MIC_COMM_FORMAT I2S_COMM_FORMAT_STAND_I2S
#define MIC_DMA_BUF_COUNT 8
#define MIC_DMA_BUF_LEN 64
#define MIC_READ_TIMEOUT_MS 20

// SPH0645 outputs ~18-bit left-aligned samples in a 32-bit frame.
#define MIC_SAMPLE_SHIFT 14

// Mouth animation tuning
#define MIC_MOUTH_OPEN_HOLD_MS 300
#define MIC_MIN_BRIGHTNESS 50
#define MIC_MAX_BRIGHTNESS 255
// Brightness smoothing (EMA on mapped brightness).
#define MIC_BRIGHTNESS_ATTACK_ALPHA 0.35f  // Rise speed (higher = faster)
#define MIC_BRIGHTNESS_RELEASE_ALPHA 0.08f // Fall speed (lower = smoother)
// Knock/tap rejection:
// Require sustained audio before opening the mouth, and ignore short impulses.
#define MIC_MIN_OPEN_MS 40
#define MIC_IMPULSE_PEAK_RATIO 6.0f
#define MIC_IMPULSE_MAX_AVG_RATIO 0.5f
#define MIC_IMPULSE_HOLDOFF_MS 80

// Envelope + ambient tracking
// Envelope smoothing factors (Exponential Moving Average):
// - Ambient EMA tracks the "room noise floor" slowly to avoid pumping.
// - Signal EMA tracks momentary loudness quickly for responsive mouth motion.
#define MIC_AMBIENT_EMA_ALPHA 0.01f      // Ambient rise speed (higher = faster)
#define MIC_AMBIENT_FALL_ALPHA 0.002f    // Ambient fall speed (lower = slower)
#define MIC_SIGNAL_EMA_ALPHA 0.3f        // Larger = faster response to voice peaks (more reactive)

// Sensitivity / thresholding:
// Amount the signal envelope must exceed the ambient estimate to be considered "speaking".
#define MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT 750.0f
// Close threshold is lower than open threshold to prevent chatter.
#define MIC_CLOSE_HYSTERESIS_RATIO 0.6f
// Limit ambient updates to values that stay well below the open threshold.
#define MIC_AMBIENT_UPDATE_BAND 0.4f

// Ambient update gating:
// Only allow the ambient estimate to drift after we've been quiet for this long.
#define MIC_AMBIENT_UPDATE_QUIET_PERIOD_MS 1000

// Ambient clamping:
// Prevents the ambient estimate from going unrealistically low/high (helps stability across environments).
#define MIC_MIN_AMBIENT_LEVEL 10000.0f
#define MIC_MAX_AMBIENT_LEVEL 30000.0f

// Safety rule:
// If the current signal is above this, we treat it as "active audio" and avoid updating ambient
// (so speech doesn't become the new noise floor).
#define MIC_MIN_SIGNAL_FOR_AMBIENT 100.0f

// Silence handling:
// When quiet, update the ambient estimate at this interval to slowly follow changing background noise.
#define MIC_AMBIENT_SILENCE_UPDATE_MS 100

// Stuck-open protection:
// If the mouth has been open continuously for this long, slowly raise the ambient floor
// toward the current signal so the threshold can recover.
#define MIC_OPEN_REBASE_MS 5000
#define MIC_OPEN_REBASE_ALPHA 0.02f

#ifndef DEBUG_MICROPHONE
// #define DEBUG_MICROPHONE 1
#endif

#endif // MIC_CONFIG_H
