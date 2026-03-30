#include "mic.h"

#include <Arduino.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"

#include "mic_config.h"
#include "mic_math.h"
#include "mic_pins.h"

static TaskHandle_t s_micTaskHandle = nullptr;
static volatile bool s_mouthOpen = false;
static volatile uint8_t s_mouthBrightness = MIC_MIN_BRIGHTNESS;
static volatile bool s_micEnabled = true;

struct MicBlockStats
{
  float rawMean = 0.0f;
  float centeredMean = 0.0f;
  float blockEnvelope = 0.0f;
  int32_t rawMin = 0;
  int32_t rawMax = 0;
};

static MicBlockStats analyzeMicBuffer(const int32_t *micBuffer, size_t frames, float &dcPrevInput, float &dcPrevOutput)
{
  MicBlockStats stats;
  if (frames == 0)
  {
    return stats;
  }

  double rawSum = 0.0;
  double centeredSum = 0.0;
  double absSum = 0.0;
  bool haveSample = false;

  for (size_t frame = 0; frame < frames; ++frame)
  {
    const size_t sampleIndex = frame * MIC_SLOT_COUNT + MIC_ACTIVE_SLOT_INDEX;
    const int32_t sample = micBuffer[sampleIndex] >> MIC_SAMPLE_SHIFT;
    const float sampleF = static_cast<float>(sample);
    const float centered = sampleF - dcPrevInput + MIC_DC_BLOCK_ALPHA * dcPrevOutput;

    dcPrevInput = sampleF;
    dcPrevOutput = centered;

    rawSum += sampleF;
    centeredSum += centered;
    absSum += std::fabs(centered);

    if (!haveSample)
    {
      stats.rawMin = sample;
      stats.rawMax = sample;
      haveSample = true;
    }
    else
    {
      if (sample < stats.rawMin)
      {
        stats.rawMin = sample;
      }
      if (sample > stats.rawMax)
      {
        stats.rawMax = sample;
      }
    }
  }

  const float frameCount = static_cast<float>(frames);
  stats.rawMean = static_cast<float>(rawSum / frameCount);
  stats.centeredMean = static_cast<float>(centeredSum / frameCount);
  stats.blockEnvelope = static_cast<float>(absSum / frameCount);
  return stats;
}

static void configureI2S()
{
  i2s_config_t i2sConfig = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = MIC_SAMPLE_RATE,
      .bits_per_sample = MIC_BITS_PER_SAMPLE,
      .channel_format = MIC_CHANNEL_FORMAT,
      .communication_format = static_cast<i2s_comm_format_t>(MIC_COMM_FORMAT),
      .intr_alloc_flags = MIC_I2S_INTR_FLAGS,
      .dma_buf_count = MIC_DMA_BUF_COUNT,
      .dma_buf_len = MIC_DMA_BUF_LEN,
      .use_apll = true,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pinConfig = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = MIC_SCK_PIN,
      .ws_io_num = MIC_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = MIC_SD_PIN};

  esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &i2sConfig, 0, nullptr);
  if (err != ESP_OK)
  {
    Serial.printf("Failed installing I2S driver: %d\n", err);
    while (true)
      ;
  }

  err = i2s_set_pin(MIC_I2S_PORT, &pinConfig);
  if (err != ESP_OK)
  {
    Serial.printf("Failed setting I2S pin: %d\n", err);
    while (true)
      ;
  }

  err = i2s_set_clk(MIC_I2S_PORT, MIC_SAMPLE_RATE, MIC_BITS_PER_SAMPLE, I2S_CHANNEL_STEREO);
  if (err != ESP_OK)
  {
    Serial.printf("Failed setting I2S clock: %d\n", err);
    while (true)
      ;
  }

  Serial.println("I2S mic driver installed.");
}

static void micTask(void *param)
{
  (void)param;

  static int32_t micBuffer[MIC_DMA_BUF_LEN * MIC_SLOT_COUNT];
  float noiseFloor = MIC_NOISE_FLOOR_INIT;
  float peakReference = MIC_PEAK_REF_INIT;
  float smoothedEnvelope = 0.0f;
  float dcPrevInput = 0.0f;
  float dcPrevOutput = 0.0f;
  unsigned long lastSpeechTime = 0;
  bool i2sRunning = true;

  auto resetMicState = [&]()
  {
    noiseFloor = MIC_NOISE_FLOOR_INIT;
    peakReference = MIC_PEAK_REF_INIT;
    smoothedEnvelope = 0.0f;
    dcPrevInput = 0.0f;
    dcPrevOutput = 0.0f;
    s_mouthBrightness = MIC_MIN_BRIGHTNESS;
    s_mouthOpen = false;
    lastSpeechTime = 0;
  };

  auto updateBrightness = [&](float normalizedEnvelope)
  {
    const float targetBrightness = micComputeBrightnessTarget(normalizedEnvelope);
    s_mouthBrightness = static_cast<uint8_t>(targetBrightness + 0.5f);
  };

  for (;;)
  {
    if (!s_micEnabled)
    {
      if (i2sRunning)
      {
        i2s_stop(MIC_I2S_PORT);
#ifdef MIC_PD_PIN
        digitalWrite(MIC_PD_PIN, LOW);
#endif
        i2sRunning = false;
      }
      resetMicState();
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (!i2sRunning)
    {
#ifdef MIC_PD_PIN
      digitalWrite(MIC_PD_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(MIC_POWER_UP_DELAY_MS));
#endif
      i2s_start(MIC_I2S_PORT);
      i2sRunning = true;
      resetMicState();
    }

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(
        MIC_I2S_PORT,
        micBuffer,
        sizeof(micBuffer),
        &bytesRead,
        pdMS_TO_TICKS(MIC_READ_TIMEOUT_MS));

    const unsigned long now = millis();

    if (err != ESP_OK || bytesRead == 0)
    {
      smoothedEnvelope = micApplyAttackReleaseEma(
          smoothedEnvelope,
          0.0f,
          MIC_SIGNAL_ATTACK_ALPHA,
          MIC_SIGNAL_RELEASE_ALPHA);
      peakReference = micUpdatePeakReference(peakReference, 0.0f);
      updateBrightness(smoothedEnvelope);

      if (s_mouthOpen && (now - lastSpeechTime > MIC_MOUTH_OPEN_HOLD_MS))
      {
        s_mouthOpen = false;
      }

#if DEBUG_MICROPHONE
      Serial.printf(">mic_raw_min:0\n");
      Serial.printf(">mic_raw_max:0\n");
      Serial.printf(">mic_raw_mean:0\n");
      Serial.printf(">mic_centered_mean:0\n");
      Serial.printf(">mic_block_env:0\n");
      Serial.printf(">mic_noise_floor:%.1f\n", noiseFloor);
      Serial.printf(">mic_speech_level:0\n");
      Serial.printf(">mic_smoothed_env:%.3f\n", smoothedEnvelope);
      Serial.printf(">mic_mouth_brightness:%u\n", s_mouthBrightness);
      Serial.printf(">mic_mouth_open:%d\n", s_mouthOpen ? 1 : 0);
#endif
      vTaskDelay(1);
      continue;
    }

    const size_t wordsRead = bytesRead / sizeof(micBuffer[0]);
    const size_t frames = wordsRead / MIC_SLOT_COUNT;
    if (frames == 0)
    {
      vTaskDelay(1);
      continue;
    }

    const MicBlockStats stats = analyzeMicBuffer(micBuffer, frames, dcPrevInput, dcPrevOutput);
    noiseFloor = micUpdateNoiseFloor(noiseFloor, stats.blockEnvelope);
    const float speechLevel = micComputeSpeechLevel(stats.blockEnvelope, noiseFloor);
    peakReference = micUpdatePeakReference(peakReference, speechLevel);
    const float normalizedSpeech = micNormalizeSpeechLevel(speechLevel, peakReference);
    smoothedEnvelope = micApplyAttackReleaseEma(
        smoothedEnvelope,
        normalizedSpeech,
        MIC_SIGNAL_ATTACK_ALPHA,
        MIC_SIGNAL_RELEASE_ALPHA);

    updateBrightness(smoothedEnvelope);

    if (micShouldOpenMouth(smoothedEnvelope, s_mouthOpen))
    {
      lastSpeechTime = now;
      s_mouthOpen = true;
    }
    else if (s_mouthOpen && (now - lastSpeechTime > MIC_MOUTH_OPEN_HOLD_MS))
    {
      s_mouthOpen = false;
    }

#if DEBUG_MICROPHONE
    Serial.printf(">mic_raw_min:%ld\n", static_cast<long>(stats.rawMin));
    Serial.printf(">mic_raw_max:%ld\n", static_cast<long>(stats.rawMax));
    Serial.printf(">mic_raw_mean:%.1f\n", stats.rawMean);
    Serial.printf(">mic_centered_mean:%.1f\n", stats.centeredMean);
    Serial.printf(">mic_block_env:%.1f\n", stats.blockEnvelope);
    Serial.printf(">mic_noise_floor:%.1f\n", noiseFloor);
    Serial.printf(">mic_speech_level:%.1f\n", speechLevel);
    Serial.printf(">mic_smoothed_env:%.3f\n", smoothedEnvelope);
    Serial.printf(">mic_mouth_brightness:%u\n", s_mouthBrightness);
    Serial.printf(">mic_mouth_open:%d\n", s_mouthOpen ? 1 : 0);
#endif

    vTaskDelay(1);
  }
}
// cppcheck-suppress unusedFunction
void micInit()
{
#ifdef MIC_PD_PIN
  pinMode(MIC_PD_PIN, OUTPUT);
  digitalWrite(MIC_PD_PIN, HIGH);
  delay(MIC_POWER_UP_DELAY_MS);
#endif

  configureI2S();
  s_micEnabled = true;

  const BaseType_t result = xTaskCreatePinnedToCore(
      micTask,
      "MicTask",
      4096,
      nullptr,
      1,
      &s_micTaskHandle,
#if portNUM_PROCESSORS > 1
      1
#else
      0
#endif
  );

  if (result != pdPASS)
  {
    Serial.println("Failed to create MicTask");
  }
}

void micSetEnabled(bool enabled)
{
  s_micEnabled = enabled;
}

bool micIsMouthOpen()
{
  return s_mouthOpen;
}

uint8_t micGetMouthBrightness()
{
  return s_mouthBrightness;
}
