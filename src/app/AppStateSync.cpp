#include "app/AppStateSync.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace
{
StaticSemaphore_t stateMutexStorage;
SemaphoreHandle_t stateMutex = nullptr;
}

void initializeAppStateSync()
{
  stateMutex = xSemaphoreCreateRecursiveMutexStatic(&stateMutexStorage);
  configASSERT(stateMutex != nullptr);
}

AppStateLock::AppStateLock()
{
  configASSERT(stateMutex != nullptr);
  locked_ = xSemaphoreTakeRecursive(stateMutex, portMAX_DELAY) == pdTRUE;
  configASSERT(locked_);
}

AppStateLock::~AppStateLock()
{
  unlock();
}

void AppStateLock::unlock()
{
  if (locked_)
  {
    locked_ = false;
    xSemaphoreGiveRecursive(stateMutex);
  }
}
