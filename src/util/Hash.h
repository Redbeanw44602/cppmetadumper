#pragma once

constexpr unsigned int H(const char* str, int h = 0) { return !str[h] ? 5381 : (H(str, h + 1) * 33) ^ str[h]; }