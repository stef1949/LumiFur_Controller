#pragma once
#include "freertos/FreeRTOS.h"
using TaskFunction_t = void (*)(void*);
BaseType_t xTaskCreate(TaskFunction_t, const char*, uint32_t, void*, UBaseType_t, void*);
void vTaskDelay(TickType_t);
void vTaskDelete(void*);
