#ifndef MIC_H
#define MIC_H

#include <cstdint>

void micInit();
void micSetEnabled(bool enabled);
bool micIsMouthOpen();
uint8_t micGetMouthBrightness();

#endif // MIC_H
