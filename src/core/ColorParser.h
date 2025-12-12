#pragma once

#include <cstdint>
#include <string>

// Lightweight RGB color struct that is independent from FastLED.
struct RgbColor
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

// Converts a color to a 6-character hexadecimal string (uppercase).
std::string colorToHexString(const RgbColor &color);

// Attempts to parse the first six hexadecimal digits in the string into a color.
// Returns true on success.
bool parseHexColorDigits(const std::string &digits, RgbColor &colorOut);

// Parses decimal components in the form "r,g,b" where each value is clamped to [0,255].
bool parseDecimalColorComponents(const std::string &input, RgbColor &colorOut);

// Parses either hexadecimal (with or without prefixes) or decimal components.
bool parseColorFromAscii(const std::string &input, RgbColor &colorOut);

// Parses arbitrary BLE payloads. Accepts ASCII decimal/hex strings or raw 3-byte RGB payloads.
bool parseColorPayload(const uint8_t *data, size_t length, RgbColor &colorOut, std::string &normalizedHex);

// Trims leading/trailing whitespace from a string.
std::string trimCopy(const std::string &input);

