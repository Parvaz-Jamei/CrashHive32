#pragma once

#include <stddef.h>

bool ch32_is_valid_key_char(char c);
void ch32_copy_key(char* destination, size_t destinationSize, const char* source);
bool ch32_make_key(char* destination, size_t destinationSize, const char* source);
