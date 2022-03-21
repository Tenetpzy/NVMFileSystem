#pragma once
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#include "CommonUtil.h"
#define L1FDSIZE 256
#define L2FDSIZE 256
#define MAXFDSIZE (L1FDSIZE * L2FDSIZE)
#define L1FDARRAYSIZE ROUND_UP(L1FDSIZE, 64)
#define L2FDARRAYSIZE ROUND_UP(L2FDSIZE, 64)

struct file;
struct fd_array;

struct fd_index_array
{
    _Atomic(uint64_t) bitset;
    struct fd_array *fd_array[64];
};

struct file_struct
{
    struct fd_index_array fd_index_array[L1FDARRAYSIZE];
    pthread_rwlock_t rwlock;
};

void file_struct_init(struct file_struct *self);

int file_struct_alloc_fd_slot(struct file_struct *self);

void file_struct_free_fd_slot(struct file_struct *self, int fd);

struct file **file_struct_access_fd_slot(struct file_struct *self, int fd);

void file_struct_destroy(struct file_struct *self);
