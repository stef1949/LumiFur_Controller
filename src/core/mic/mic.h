#ifndef MIC_H
#define MIC_H

#include <cstdint>

void micInit();
void micSetEnabled(bool enabled);
uint8_t micGetMouthOpenness();
uint8_t micGetMouthBrightness();
uint32_t micGetTaskStackHighWaterMark();

#endif // MIC_H
