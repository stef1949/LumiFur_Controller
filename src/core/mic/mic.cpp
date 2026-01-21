#include "mic.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"

#include "mic_config.h"
#include "mic_pins.h"

static TaskHandle_t s_micTaskHandle = nullptr;
static volatile bool s_mouthOpen = false;
static volatile uint8_t s_mouthBrightness = MIC_MIN_BRIGHTNESS;
static volatile bool s_micEnabled = true;

static void configureI2S()
{
  i2s_config_t i2sConfig = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = MIC_SAMPLE_RATE,
      .bits_per_sample = MIC_BITS_PER_SAMPLE,
      .channel_format = MIC_CHANNEL_FORMAT,
      .communication_format = static_cast<i2s_comm_format_t>(MIC_COMM_FORMAT),
      .intr_alloc_flags = 1,
      .dma_buf_count = MIC_DMA_BUF_COUNT,
      .dma_buf_len = MIC_DMA_BUF_LEN,
      .use_apll = true,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pinConfig = {
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

  Serial.println("I2S mic driver installed.");
}

static void micTask(void *param)
{
  (void)param;

  static int32_t micBuffer[MIC_DMA_BUF_LEN];
  float ambientNoiseLevel = (MIC_MIN_AMBIENT_LEVEL + MIC_MAX_AMBIENT_LEVEL) * 0.5f;
  float smoothedSignalLevel = 0.0f;
  float brightnessEma = static_cast<float>(MIC_MIN_BRIGHTNESS);
  unsigned long lastSoundTime = 0;
  unsigned long lastAmbientUpdateTime = 0;
  unsigned long lastFrameTime = 0;
  unsigned long activeMs = 0;
  unsigned long impulseHoldUntil = 0;
  unsigned long mouthOpenSince = 0;
  bool wasMouthOpen = false;
  bool i2sRunning = true;

  auto resetMicState = [&]()
  {
    ambientNoiseLevel = (MIC_MIN_AMBIENT_LEVEL + MIC_MAX_AMBIENT_LEVEL) * 0.5f;
    smoothedSignalLevel = 0.0f;
    brightnessEma = static_cast<float>(MIC_MIN_BRIGHTNESS);
    s_mouthBrightness = MIC_MIN_BRIGHTNESS;
    s_mouthOpen = false;
    lastSoundTime = 0;
    lastAmbientUpdateTime = 0;
    lastFrameTime = 0;
    activeMs = 0;
    impulseHoldUntil = 0;
    mouthOpenSince = 0;
    wasMouthOpen = false;
  };

  auto updateBrightness = [&](float targetBrightness)
  {
    const float alpha = (targetBrightness > brightnessEma) ? MIC_BRIGHTNESS_ATTACK_ALPHA : MIC_BRIGHTNESS_RELEASE_ALPHA;
    brightnessEma += alpha * (targetBrightness - brightnessEma);
    brightnessEma = constrain(brightnessEma,
                              static_cast<float>(MIC_MIN_BRIGHTNESS),
                              static_cast<float>(MIC_MAX_BRIGHTNESS));
    s_mouthBrightness = static_cast<uint8_t>(brightnessEma + 0.5f);
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
    unsigned long elapsedMs = 0;
    if (lastFrameTime != 0 && now >= lastFrameTime)
    {
      elapsedMs = now - lastFrameTime;
    }
    lastFrameTime = now;

    if (err != ESP_OK || bytesRead == 0)
    {
      if (s_mouthOpen && (now - lastSoundTime > MIC_MOUTH_OPEN_HOLD_MS))
      {
        s_mouthOpen = false;
      }

      smoothedSignalLevel *= (1.0f - MIC_SIGNAL_EMA_ALPHA);
      updateBrightness(static_cast<float>(MIC_MIN_BRIGHTNESS));
      activeMs = 0;

#if DEBUG_MICROPHONE
      Serial.printf(">mic_avg_abs_signal:0\n");
      Serial.printf(">mic_smoothed_signal:%.0f\n", smoothedSignalLevel);
      Serial.printf(">mic_ambient_noise:%.0f\n", ambientNoiseLevel);
      Serial.printf(">mic_trigger_threshold:%.0f\n", ambientNoiseLevel + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT);
      Serial.printf(">mic_maw_brightness:%u\n", s_mouthBrightness);
      Serial.printf(">mic_mouth_open:%d\n", s_mouthOpen ? 1 : 0);
#endif
      wasMouthOpen = s_mouthOpen;
      vTaskDelay(1);
      continue;
    }

    size_t samples = bytesRead / sizeof(micBuffer[0]);
    if (samples == 0)
    {
      wasMouthOpen = s_mouthOpen;
      vTaskDelay(1);
      continue;
    }

    uint64_t sumAbs = 0;
    uint32_t peakAbs = 0;
    for (size_t i = 0; i < samples; ++i)
    {
      int32_t shifted = micBuffer[i] >> MIC_SAMPLE_SHIFT;
      uint32_t absVal = shifted < 0 ? static_cast<uint32_t>(-shifted) : static_cast<uint32_t>(shifted);
      sumAbs += absVal;
      if (absVal > peakAbs)
      {
        peakAbs = absVal;
      }
    }

    float currentAvgAbsSignal = static_cast<float>(sumAbs) / static_cast<float>(samples);
    const float openThreshold = ambientNoiseLevel + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT;
    const float impulseAvgLimit = ambientNoiseLevel + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_IMPULSE_MAX_AVG_RATIO;
    const float peakToAvg = (currentAvgAbsSignal > 1.0f) ? (static_cast<float>(peakAbs) / currentAvgAbsSignal) : 0.0f;
    const bool isImpulse = (peakToAvg >= MIC_IMPULSE_PEAK_RATIO) && (currentAvgAbsSignal < impulseAvgLimit);

    if (isImpulse)
    {
      impulseHoldUntil = now + MIC_IMPULSE_HOLDOFF_MS;
    }

    if (now < impulseHoldUntil)
    {
      if (s_mouthOpen && (now - lastSoundTime > MIC_MOUTH_OPEN_HOLD_MS))
      {
        s_mouthOpen = false;
      }

      smoothedSignalLevel *= (1.0f - MIC_SIGNAL_EMA_ALPHA);
      updateBrightness(static_cast<float>(MIC_MIN_BRIGHTNESS));
      activeMs = 0;
      wasMouthOpen = s_mouthOpen;
      vTaskDelay(1);
      continue;
    }

    smoothedSignalLevel = MIC_SIGNAL_EMA_ALPHA * currentAvgAbsSignal + (1.0f - MIC_SIGNAL_EMA_ALPHA) * smoothedSignalLevel;

    const float closeThreshold = ambientNoiseLevel + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_CLOSE_HYSTERESIS_RATIO;

    if (smoothedSignalLevel > openThreshold)
    {
      activeMs += elapsedMs;
    }
    else
    {
      activeMs = 0;
    }

    const bool qualifies = activeMs >= MIC_MIN_OPEN_MS;

    if (smoothedSignalLevel > closeThreshold && (s_mouthOpen || qualifies))
    {
      lastSoundTime = now;
    }

    if (!s_mouthOpen && smoothedSignalLevel > openThreshold && qualifies)
    {
      s_mouthOpen = true;
    }
    else if (s_mouthOpen && (now - lastSoundTime > MIC_MOUTH_OPEN_HOLD_MS))
    {
      s_mouthOpen = false;
    }

    const bool isQuietPeriod = (now - lastSoundTime > MIC_AMBIENT_UPDATE_QUIET_PERIOD_MS);
    const float ambientUpdateLimit = ambientNoiseLevel + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_AMBIENT_UPDATE_BAND;
    if (!s_mouthOpen && smoothedSignalLevel <= ambientUpdateLimit && (now - lastAmbientUpdateTime > MIC_AMBIENT_SILENCE_UPDATE_MS))
    {
      float alpha = 0.0f;
      if (smoothedSignalLevel >= ambientNoiseLevel)
      {
        alpha = MIC_AMBIENT_EMA_ALPHA;
      }
      else if (isQuietPeriod && currentAvgAbsSignal >= MIC_MIN_SIGNAL_FOR_AMBIENT)
      {
        alpha = MIC_AMBIENT_FALL_ALPHA;
      }

      if (alpha > 0.0f)
      {
        ambientNoiseLevel = ambientNoiseLevel + alpha * (smoothedSignalLevel - ambientNoiseLevel);
        ambientNoiseLevel = constrain(ambientNoiseLevel, MIC_MIN_AMBIENT_LEVEL, MIC_MAX_AMBIENT_LEVEL);
        lastAmbientUpdateTime = now;
      }
    }

    float signalAboveAmbient = smoothedSignalLevel - ambientNoiseLevel;
    if (signalAboveAmbient < 0.0f)
      signalAboveAmbient = 0.0f;

    float brightnessMappingRange = MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    float normalized = signalAboveAmbient / brightnessMappingRange;
    if (normalized > 1.0f)
      normalized = 1.0f;

    const float targetBrightness = static_cast<float>(MIC_MIN_BRIGHTNESS) +
                                   (static_cast<float>(MIC_MAX_BRIGHTNESS - MIC_MIN_BRIGHTNESS) * normalized);
    updateBrightness(targetBrightness);

    if (s_mouthOpen)
    {
      if (!wasMouthOpen)
      {
        mouthOpenSince = now;
      }
      if ((now - mouthOpenSince) > MIC_OPEN_REBASE_MS && smoothedSignalLevel > ambientNoiseLevel)
      {
        ambientNoiseLevel = MIC_OPEN_REBASE_ALPHA * smoothedSignalLevel + (1.0f - MIC_OPEN_REBASE_ALPHA) * ambientNoiseLevel;
        ambientNoiseLevel = constrain(ambientNoiseLevel, MIC_MIN_AMBIENT_LEVEL, MIC_MAX_AMBIENT_LEVEL);
      }
    }
    wasMouthOpen = s_mouthOpen;

#if DEBUG_MICROPHONE
    Serial.printf(">mic_avg_abs_signal:%.0f\n", currentAvgAbsSignal);
    Serial.printf(">mic_smoothed_signal:%.0f\n", smoothedSignalLevel);
    Serial.printf(">mic_ambient_noise:%.0f\n", ambientNoiseLevel);
    Serial.printf(">mic_trigger_threshold:%.0f\n", openThreshold);
    Serial.printf(">mic_maw_brightness:%u\n", s_mouthBrightness);
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
#if defined(CONFIG_ARDUINO_RUNNING_CORE)
      CONFIG_ARDUINO_RUNNING_CORE
#else
      1
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
