#pragma once
#include <stdint.h>
#include "CommonUtil.h"
#define L1FDSIZE 256
#define L2FDSIZE 256

struct file;
struct fd_array;

struct fd_index_array {
	uint64_t bitset;
	struct fd_array *fd_array[64];
};



struct file_struct {
	struct fd_index_array fd_index_array[ROUND_UP(L1FDSIZE,64)];
};


void file_struct_init(struct file_struct *self);

int file_struct_alloc_fd_slot(struct file_struct *self);

void file_struct_free_fd_slot(struct file_struct *self, int fd);

struct file *file_struct_access_fd_slot(struct file_struct *self, int fd);

void file_struct_destroy(struct file_struct *self);
