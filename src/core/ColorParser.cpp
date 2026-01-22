#include "core/ColorParser.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

std::string trimCopy(const std::string &input)
{
  size_t start = 0;
  size_t end = input.size();
  while (start < end && std::isspace(static_cast<unsigned char>(input[start])))
  {
    ++start;
  }
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
  {
    --end;
  }
  return input.substr(start, end - start);
}

std::string colorToHexString(const RgbColor &color)
{
  char buffer[7];
  snprintf(buffer, sizeof(buffer), "%02X%02X%02X", color.r, color.g, color.b);
  return std::string(buffer);
}

bool parseHexColorDigits(const std::string &digits, RgbColor &colorOut)
{
  if (digits.size() < 6)
  {
    return false;
  }
  uint8_t rHigh, rLow, gHigh, gLow, bHigh, bLow;
  auto hexCharToValue = [](char c, uint8_t &value) -> bool {
    if (c >= '0' && c <= '9')
    {
      value = static_cast<uint8_t>(c - '0');
      return true;
    }
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'F')
    {
      value = static_cast<uint8_t>(10 + (c - 'A'));
      return true;
    }
    return false;
  };

  if (!hexCharToValue(digits[0], rHigh) || !hexCharToValue(digits[1], rLow) ||
      !hexCharToValue(digits[2], gHigh) || !hexCharToValue(digits[3], gLow) ||
      !hexCharToValue(digits[4], bHigh) || !hexCharToValue(digits[5], bLow))
  {
    return false;
  }
  colorOut.r = static_cast<uint8_t>((rHigh << 4) | rLow);
  colorOut.g = static_cast<uint8_t>((gHigh << 4) | gLow);
  colorOut.b = static_cast<uint8_t>((bHigh << 4) | bLow);
  return true;
}

bool parseDecimalColorComponents(const std::string &input, RgbColor &colorOut)
{
  std::vector<int> values;
  const char *ptr = input.c_str();
  char *endPtr = nullptr;
  while (*ptr != '\0')
  {
    if (*ptr == ',' || *ptr == ';')
    {
      ++ptr;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(*ptr)))
    {
      ++ptr;
      continue;
    }

    long parsed = strtol(ptr, &endPtr, 10);
    if (ptr == endPtr)
    {
      return false;
    }
    values.push_back(static_cast<int>(parsed));
    if (values.size() > 3)
    {
      break;
    }
    ptr = endPtr;
  }

  if (values.size() != 3)
  {
    return false;
  }

  auto clampComponent = [](int value) -> uint8_t {
    if (value < 0)
      value = 0;
    if (value > 255)
      value = 255;
    return static_cast<uint8_t>(value);
  };

  colorOut.r = clampComponent(values[0]);
  colorOut.g = clampComponent(values[1]);
  colorOut.b = clampComponent(values[2]);
  return true;
}

bool parseColorFromAscii(const std::string &input, RgbColor &colorOut)
{
  std::string trimmed = trimCopy(input);
  if (trimmed.empty())
  {
    return false;
  }

  bool hasSeparator = false;
  bool hasHexLetter = false;
  bool hexPreferred = false;
  size_t startIndex = 0;
  if (trimmed[0] == '#')
  {
    hexPreferred = true;
    startIndex = 1;
  }
  else if (trimmed.size() > 1 && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X'))
  {
    hexPreferred = true;
    startIndex = 2;
  }

  std::string hexDigits;
  hexDigits.reserve(trimmed.size());
  for (size_t i = startIndex; i < trimmed.size(); ++i)
  {
    unsigned char c = static_cast<unsigned char>(trimmed[i]);
    if (c == ',' || c == ';' || std::isspace(c))
    {
      hasSeparator = true;
      continue;
    }
    if (std::isxdigit(c))
    {
      if (std::isalpha(c))
      {
        hasHexLetter = true;
      }
      hexDigits.push_back(static_cast<char>(std::toupper(c)));
    }
  }

  if (hexDigits.size() >= 6 && (hexPreferred || hasHexLetter || !hasSeparator))
  {
    return parseHexColorDigits(hexDigits.substr(0, 6), colorOut);
  }

  if (hexPreferred)
  {
    return false;
  }

  return parseDecimalColorComponents(trimmed, colorOut);
}

bool parseColorPayload(const uint8_t *data, size_t length, RgbColor &colorOut, std::string &normalizedHex)
{
  if (!data || length == 0)
  {
    return false;
  }

  const std::string ascii(reinterpret_cast<const char *>(data), length);
  if (parseColorFromAscii(ascii, colorOut))
  {
    normalizedHex = colorToHexString(colorOut);
    return true;
  }

  if (length == 3)
  {
    colorOut.r = data[0];
    colorOut.g = data[1];
    colorOut.b = data[2];
    normalizedHex = colorToHexString(colorOut);
    return true;
  }

  return false;
}
