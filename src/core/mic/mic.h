#ifndef MIC_H
#define MIC_H

#include <cstdint>

void micInit();
bool micIsMouthOpen();
uint8_t micGetMouthBrightness();

#endif // MIC_H
