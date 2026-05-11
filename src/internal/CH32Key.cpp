#include "CH32Key.h"

#include <string.h>

bool ch32_is_valid_key_char(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == '_' || c == '-' || c == '.';
}

void ch32_copy_key(char* destination, size_t destinationSize, const char* source) {
  if (destination == nullptr || destinationSize == 0U) {
    return;
  }

  size_t i = 0U;
  if (source != nullptr) {
    for (; i + 1U < destinationSize && source[i] != '\0'; ++i) {
      const char c = source[i];
      destination[i] = ch32_is_valid_key_char(c) ? c : '_';
    }
  }

  destination[i] = '\0';
  for (++i; i < destinationSize; ++i) {
    destination[i] = '\0';
  }
}

bool ch32_make_key(char* destination, size_t destinationSize, const char* source) {
  if (destination == nullptr || destinationSize == 0U || source == nullptr || source[0] == '\0') {
    return false;
  }

  size_t i = 0U;
  for (; source[i] != '\0'; ++i) {
    if (i + 1U >= destinationSize) {
      destination[0] = '\0';
      return false;
    }
    const char c = source[i];
    destination[i] = ch32_is_valid_key_char(c) ? c : '_';
  }

  if (i == 0U) {
    destination[0] = '\0';
    return false;
  }

  destination[i] = '\0';
  for (++i; i < destinationSize; ++i) {
    destination[i] = '\0';
  }
  return true;
}
