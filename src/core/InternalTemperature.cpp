#include "core/InternalTemperature.h"

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_idf_version.h"

#if defined(__has_include)
#if __has_include("driver/temperature_sensor.h")
#define LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER 1
#endif
#endif

#ifndef LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER
#define LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER 0
#endif

#if LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER
#include "driver/temperature_sensor.h"
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER 1
#include "driver/temp_sensor.h"
#endif
#else
#define LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER 0
#endif

#ifndef LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER
#define LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER 0
#endif

namespace
{
#if defined(ESP_PLATFORM) && LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER
temperature_sensor_handle_t sTempSensor = nullptr;
bool sTempSensorEnabled = false;
#elif defined(ESP_PLATFORM) && LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER
bool sTempSensorStarted = false;
#endif
} // namespace

bool internalTemperatureInit()
{
#if defined(ESP_PLATFORM) && LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER
  if (sTempSensor == nullptr)
  {
    temperature_sensor_config_t tempSensor = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    esp_err_t err = temperature_sensor_install(&tempSensor, &sTempSensor);
    if (err != ESP_OK)
    {
      sTempSensor = nullptr;
      return false;
    }
  }

  if (!sTempSensorEnabled)
  {
    esp_err_t err = temperature_sensor_enable(sTempSensor);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
      return false;
    }
    sTempSensorEnabled = true;
  }

  return true;
#elif defined(ESP_PLATFORM) && LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER
  if (sTempSensorStarted)
  {
    return true;
  }

  temp_sensor_config_t tempSensor = TSENS_CONFIG_DEFAULT();
  tempSensor.dac_offset = TSENS_DAC_L2;

  esp_err_t err = temp_sensor_set_config(tempSensor);
  if (err != ESP_OK)
  {
    return false;
  }

  err = temp_sensor_start();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
  {
    return false;
  }

  sTempSensorStarted = true;
  return true;
#else
  return false;
#endif
}

bool internalTemperatureReadCelsius(float &celsiusOut)
{
  if (!internalTemperatureInit())
  {
    return false;
  }

#if defined(ESP_PLATFORM) && LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER
  return temperature_sensor_get_celsius(sTempSensor, &celsiusOut) == ESP_OK;
#elif defined(ESP_PLATFORM) && LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER
  return temp_sensor_read_celsius(&celsiusOut) == ESP_OK;
#else
  (void)celsiusOut;
  return false;
#endif
}

const char *internalTemperatureDriverName()
{
#if defined(ESP_PLATFORM) && LF_HAS_NEW_TEMPERATURE_SENSOR_DRIVER
  return "driver/temperature_sensor.h";
#elif defined(ESP_PLATFORM) && LF_HAS_LEGACY_TEMPERATURE_SENSOR_DRIVER
  return "driver/temp_sensor.h";
#elif defined(ESP_PLATFORM)
  return "driver/temperature_sensor.h unavailable";
#else
  return "unavailable";
#endif
}
