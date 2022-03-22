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

typedef _Atomic(uint64_t) file_struct_bitset_t;
typedef _Atomic(struct fd_array *) atomic_fd_array;

struct file;
struct fd_array;

struct fd_index_array
{
    file_struct_bitset_t bitset;
    atomic_fd_array fd_array[64];
};

struct file_struct
{
    struct fd_index_array fd_index_array[L1FDARRAYSIZE];
};

//初始化file_struct
void file_struct_init(struct file_struct *self);

//找到空闲位置会返回其位置(>=0)，没找到返回-1
int file_struct_alloc_fd_slot(struct file_struct *self);

//释放指定位置fd
void file_struct_free_fd_slot(struct file_struct *self, int fd);

//根据fd获取file**
struct file **file_struct_access_fd_slot(struct file_struct *self, int fd);

//释放file_struct资源
void file_struct_destroy(struct file_struct *self);
