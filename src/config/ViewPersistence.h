#pragma once
#include <cstdint>
void initializeViewPersistence(uint8_t view);
void flushPendingLastViewPersist(unsigned long nowMs);
