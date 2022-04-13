#pragma once

#define ROUND_DOWN(x, y) ((x) / (y))

#define ROUND_UP(x, y) ROUND_DOWN(x + y - 1, y)

#define RIGHT_MOST_ZERO_LL(ullval) __builtin_ctzll(~(unsigned long long)(ullval))

#define RIGHT_MOST_ZERO(val) __builtin_ctz(~(unsigned int)(val))

#define IS_NULL_PTR(ptr) (NULL == (ptr))

#define COUNT_ONE_LL(ullval) __builtin_popcountll(ullval)

#define OVERLOAD_1_3_ARG(_1, _2, _3, FUNC_NAME, ...) FUNC_NAME

#define OVERLOAD_1_2_ARG(_1, _2, FUNC_NAME, ...) FUNC_NAME

#define OVERLOAD_0_1_ARG(_1, _2, ...) OVERLOAD_1_2_ARG(NULL, ##__VA_ARGS__, _1, _2)
