#pragma once
#include <stdatomic.h>

typedef atomic_bool spinlock_t;

#define SPIN_CLEAR ((_Bool)0)
#define SPIN_SET ((_Bool)1)

#define init_spin(x)                   \
    do {                               \
        atomic_init(&(x), SPIN_CLEAR); \
    } while (0)

#define lock_spin(x)                                                   \
    do {                                                               \
        _Bool expect = SPIN_CLEAR;                                     \
        while (atomic_compare_exchange_weak(&(x), &expect, SPIN_SET)); \
    } while (0)

#define unlock_spin(x)                  \
    do {                                \
        atomic_store(&(x), SPIN_CLEAR); \
    } while (0)
