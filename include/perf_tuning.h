#pragma once

// HUB75 render tuning
#ifndef LF_HUB75_COLOR_DEPTH_BITS
#define LF_HUB75_COLOR_DEPTH_BITS 7
#endif

#ifndef LF_HUB75_MIN_REFRESH_RATE_HZ
#define LF_HUB75_MIN_REFRESH_RATE_HZ 120
#endif

#ifndef LF_HUB75_LATCH_BLANKING
#define LF_HUB75_LATCH_BLANKING 1
#endif

#ifndef LF_HUB75_CLKPHASE
#define LF_HUB75_CLKPHASE false
#endif

// Face/plasma pacing
#ifndef LF_FACE_VIEW_FRAME_INTERVAL_MS
#define LF_FACE_VIEW_FRAME_INTERVAL_MS 8UL
#endif

// BLE worker sizing
#ifndef LF_BLE_WORKER_QUEUE_LENGTH
#define LF_BLE_WORKER_QUEUE_LENGTH 12
#endif

#ifndef LF_BLE_WORK_ITEM_PAYLOAD_MAX
#define LF_BLE_WORK_ITEM_PAYLOAD_MAX 512
#endif

#ifndef LF_BLE_WORKER_STACK_SIZE
#define LF_BLE_WORKER_STACK_SIZE 6144
#endif

#ifndef LF_BLE_WORKER_PRIORITY
#define LF_BLE_WORKER_PRIORITY 1
#endif

// OTA intentionally trades animation cadence for BLE/flash throughput.
#ifndef LF_OTA_ACTIVE_FRAME_INTERVAL_MS
#define LF_OTA_ACTIVE_FRAME_INTERVAL_MS 25UL
#endif

// Microphone I2S tuning
#ifndef LF_MIC_DMA_BUF_COUNT
#define LF_MIC_DMA_BUF_COUNT 4
#endif

#ifndef LF_MIC_DMA_BUF_LEN
#define LF_MIC_DMA_BUF_LEN 128
#endif

// APDS9960 polling cadence
#ifndef LF_APDS_LUX_SAMPLE_INTERVAL_MS
#define LF_APDS_LUX_SAMPLE_INTERVAL_MS 250UL
#endif

#ifndef LF_APDS_PROX_SAMPLE_INTERVAL_MS
#define LF_APDS_PROX_SAMPLE_INTERVAL_MS 250UL
#endif

#ifndef LF_APDS_USE_CLEAR_ONLY_LUX
#define LF_APDS_USE_CLEAR_ONLY_LUX 1
#endif

// Input / state persistence tuning
#ifndef LF_LAST_VIEW_PERSIST_DELAY_MS
#define LF_LAST_VIEW_PERSIST_DELAY_MS 750UL
#endif

#ifndef LF_SHAKE_SUPPRESS_AFTER_BUTTON_MS
#define LF_SHAKE_SUPPRESS_AFTER_BUTTON_MS 450UL
#endif

// Performance instrumentation defaults
#ifndef LF_PERF_MONITORING_DEFAULT
#define LF_PERF_MONITORING_DEFAULT 0
#endif

#ifndef LF_PERF_LOGGING_DEFAULT
#define LF_PERF_LOGGING_DEFAULT 1
#endif
