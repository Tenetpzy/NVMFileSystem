#include "file_struct.h"

#include <stddef.h>

struct fileset
{
    _Atomic(uint64_t) bitset;
    struct file *files[64];
};

struct fd_array
{
    struct fileset fileset[L2FDARRAYSIZE];
};

static struct fd_array **find_fd_array(struct file_struct *self, int L1Index)
{
    return &self->fd_index_array[L1Index >> 6].fd_array[L1Index & 63];
}

static struct file **find_file(struct fd_array *self, int L2Index)
{
    return &self->fileset[L2Index >> 6].files[L2Index & 63];
}

static struct fd_array **find_empty_fd_array(struct file_struct *self)
{
    struct fd_index_array *it;
    for (int i = 0; i < L1FDARRAYSIZE; i++)
    {
        it = &self->fd_index_array[i];
        uint64_t bitset = it->bitset;
        if (bitset == UINT64_MAX)
        {
            // no empty slot
            break;
        }
        int firstempty = RIGHT_MOST_ZERO_LL(bitset);
    }
    return NULL;
}

void file_struct_init(struct file_struct *self)
{
    pthread_rwlock_init(&self->rwlock, NULL);
    for (int i = 0; i < L1FDARRAYSIZE; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            self->fd_index_array[i].fd_array[j] = NULL;
        }
        self->fd_index_array[i].bitset = 0;
    }
}

//找到空闲位置会返回其位置(>=0)，没找到返回-1
int file_struct_alloc_fd_slot(struct file_struct *self) {}

struct file **file_struct_access_fd_slot(struct file_struct *self, int fd)
{
    if (fd < 0 || fd >= MAXFDSIZE)
    {
        return NULL;
    }
    int L1Index = fd / L2FDSIZE;
    int L2Index = fd % L2FDSIZE;
    struct fd_array **tmp = find_fd_array(self, L1Index);
    if (*tmp == NULL)
    {
        return NULL;
    }
    return find_file(*tmp, L2Index);
}