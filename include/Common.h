#pragma once
#include <stdint.h>

#ifndef __off_t_defined
typedef uint64_t off_t;
#define __off_t_defined
#endif

#ifndef __mode_t_defined
typedef uint32_t mode_t;
#define __mode_t_defined
#endif

typedef uint64_t ino_t;