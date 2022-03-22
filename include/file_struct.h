#pragma once
#include <pthread.h>
#include <stdint.h>

#include "CommonUtil.h"
#define L1FDSIZE 256
#define L2FDSIZE 256
#define MAXFDSIZE (L1FDSIZE * L2FDSIZE)
#define L1FDARRAYSIZE ROUND_UP(L1FDSIZE, 64)
#define L2FDARRAYSIZE ROUND_UP(L2FDSIZE, 64)

struct file;

struct file_struct;

//新file_struct
struct file_struct *file_struct_new();

//找到空闲位置会返回其位置(>=0)，没找到返回-1
int file_struct_alloc_fd_slot(struct file_struct *self);

//释放指定位置fd
void file_struct_free_fd_slot(struct file_struct *self, int fd);

//根据fd获取file**
struct file **file_struct_access_fd_slot(struct file_struct *self, int fd);

//释放file_struct申请的资源
void file_struct_free(struct file_struct *self);
