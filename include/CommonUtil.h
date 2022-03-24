#pragma once

#define ROUND_DOWN(x, y) ((x) / (y))

#define ROUND_UP(x, y) ROUND_DOWN(x + y - 1, y)

#define RIGHT_MOST_ZERO_LL(ullval) __builtin_ctzll(~(unsigned long long)(ullval))

#define RIGHT_MOST_ZERO(val) __builtin_ctz(~(unsigned int)(val))